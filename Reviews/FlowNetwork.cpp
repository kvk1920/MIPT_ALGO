#include <vector>
#include <queue>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <memory>
#include <functional>

class Network
{
public:
    constexpr static int None = -1;
    constexpr static int Inf = 1000000;
    struct edge_t
    {
        int startVertex = None;
        int finishVertex = None;
        int capacity = 0;
        int flow = 0;

        int residualCapacity() const
        {
            return capacity - flow;
        }

        int goThroughEdge(int fromVertex) const { return startVertex == fromVertex ? finishVertex : startVertex; }
    };
private:
    std::vector<int>        lastEdge_;
    std::vector<int>        previousEdge_;
    std::vector<edge_t>     edges_;

    int                     source_;
    int                     sink_;

    void insertEdgeImpl(int startVertex, int finishVertex, int capacity)
    {
        previousEdge_.push_back(lastEdge_.at(startVertex));
        lastEdge_.at(startVertex) = static_cast<int>(edges_.size());
        edges_.push_back({startVertex, finishVertex, capacity});
    }
public:
    Network() = default;

    Network(size_t N, int source, int sink)
        : lastEdge_(N, None)
        , source_(source)
        , sink_(sink)
    {}

    void insertEdge(int startVertex, int finishVertex, int capacity, bool isDirected = true)
    {
        insertEdgeImpl(startVertex, finishVertex, capacity);
        insertEdgeImpl(finishVertex, startVertex, isDirected ? 0 : capacity);
    }
private:
    template <bool isBackEdgeIterator>
    class EdgeIteratorImpl : public std::iterator<std::forward_iterator_tag, edge_t>
    {
    private:
        std::vector<edge_t>*    edges_ = nullptr;
        std::vector<int>*       previousEdge_ = nullptr;
        int                     edgeId_ = None;

        EdgeIteratorImpl(
                std::vector<edge_t>* edges = nullptr,
                std::vector<int>* previousEdge = nullptr,
                int edgeId = None)
            : edges_(edges)
            , previousEdge_(previousEdge)
            , edgeId_(edgeId)
        {}
    public:
        friend Network;

        int getEdgeId() const {
            if constexpr (isBackEdgeIterator)
                return edgeId_ ^ 1;
            else
                return edgeId_;
        }

        bool isStraight() { return getEdgeId() % 2 == 0; }

        edge_t& getBackEdge() const
        {
            if constexpr (isBackEdgeIterator)
                return edges_->at(edgeId_);
            else
                return edges_->at(edgeId_ ^ 1);
        }

        edge_t& getEdge() const
        {
            if constexpr (isBackEdgeIterator)
                return edges_->at(edgeId_ ^ 1);
            else
                return edges_->at(edgeId_);
        }

        auto& operator++()
        {
            edgeId_ = previousEdge_->at(edgeId_);
            return *this;
        }

        auto operator++(int)
        {
            auto result(*this);
            operator++();
            return result;
        }

        edge_t& operator*() const { return getEdge(); }
        edge_t* operator->() const { return &getEdge(); }

        explicit operator bool() const { return edgeId_ != None; }

        bool operator==(const EdgeIteratorImpl& other) const
        {
            if (!operator bool() && !other)
                return true;
            return edges_ == other.edges_ && edgeId_ == other.edgeId_;
        }

        bool operator!=(const EdgeIteratorImpl& other) const { return !(operator==(other)); }

        void pushFlow(int flow) const
        {
            getEdge().flow += flow;
            getBackEdge().flow -= flow;
        }
    };
public:
    using EdgeIterator = EdgeIteratorImpl<false>;
    using BackEdgeIterator = EdgeIteratorImpl<true>;

    auto begin() { return edges_.begin(); }
    auto end() { return edges_.end(); }
    auto begin() const { return edges_.begin(); }
    auto end() const { return edges_.begin(); }

    template <typename Iterator>
    struct view_t
    {
        Iterator startIterator;
        Iterator finishIterator;
        Iterator begin() const { return startIterator; }
        Iterator end() const { return finishIterator; }
        void popLastEdge() { ++startIterator; }
        bool empty() const { return startIterator == finishIterator; }
    };

    auto vertexEdgeList(int vertex) { return view_t<EdgeIterator>{{&edges_, &previousEdge_, lastEdge_.at(vertex)}, {}};}
    auto vertexBackEdgeList(int vertex) { return view_t<BackEdgeIterator>{{&edges_, &previousEdge_, lastEdge_.at(vertex)}, {}};}

    size_t size() const { return lastEdge_.size(); }

    void clear()
    {
        for (auto& edge : edges_)
            edge.flow = 0;
    }

    int source() const { return source_; }
    int sink() const { return sink_; }
};

class FlowFindingAlgorithm
{
protected:
    Network network_;
    int result_ = 0;
    size_t size_ = 0;

public:
    FlowFindingAlgorithm() = default;

    int result() { return result_; }
    void reset() { network_.clear(); result_ = 0; }

    void loadNetwork(Network&& network) { network_ = std::move(network); size_ = network_.size(); }
    void storeNetwork(Network& network) { network = std::move(network_); size_ = 0; }

    virtual void run() = 0;
};

class MalhotraKumarMaheshwari : public FlowFindingAlgorithm
{
private:
    std::vector<int>
            incomePhi_,
            outcomePhi_,
            slice_,
            addedFlow_;

    std::vector<Network::view_t<Network::EdgeIterator>> edgeLists_;
    std::vector<Network::view_t<Network::BackEdgeIterator>> backEdgeLists_;

    enum class VertexState : int { Valid, CandidateToDelete, Deleted };

    std::vector<VertexState> vertexStates_;
    std::vector<int> vertexexToDelete_;

    int phi(int vertex) { return std::min(incomePhi_.at(vertex), outcomePhi_.at(vertex)); }

    void prepare()
    {
        incomePhi_.resize(size_);
        outcomePhi_.resize(size_);
        slice_.resize(size_);
        edgeLists_.resize(size_);
        backEdgeLists_.resize(size_);
        vertexStates_.resize(size_);
        vertexexToDelete_.clear();
        addedFlow_.resize(size_);
    }

    void buildSlices()
    {
        std::queue<int> bfsQueue({network_.source()});
        slice_.assign(size_, size_);
        slice_.at(network_.source()) = 0;
        while (!bfsQueue.empty())
        {
            int vertex(bfsQueue.front());
            bfsQueue.pop();
            for (Network::edge_t& edge : network_.vertexEdgeList(vertex))
                if (edge.residualCapacity() > 0 && slice_.at(edge.goThroughEdge(vertex)) == size_)
                {
                    bfsQueue.push(edge.goThroughEdge(vertex));
                    slice_.at(edge.goThroughEdge(vertex)) = slice_.at(vertex) + 1;
                }
        }
    }

    void markInvalidVertex(int vertex)
    {
        if (vertexStates_.at(vertex) == VertexState::Valid)
        {
            vertexexToDelete_.push_back(vertex);
            vertexStates_.at(vertex) = VertexState::CandidateToDelete;
        }
    }

    void deleteInvalidVertexes()
    {
        while (!vertexexToDelete_.empty())
        {
            int vertex(vertexexToDelete_.back());
            vertexexToDelete_.pop_back();
            for (Network::edge_t& edge : edgeLists_.at(vertex))
                if (isValidEdge(edge))
                {
                    int nextVertex(edge.goThroughEdge(vertex));
                    if ((incomePhi_.at(nextVertex) -= edge.residualCapacity()) == 0)
                        markInvalidVertex(nextVertex);
                }
            for (Network::edge_t& edge : backEdgeLists_.at(vertex))
                if (isValidEdge(edge))
                {
                    int prevVertex(edge.goThroughEdge(vertex));
                    if ((outcomePhi_.at(prevVertex) -= edge.residualCapacity()) == 0)
                        markInvalidVertex(prevVertex);
                }
        }
    }

    void validateSlices()
    {
        std::queue<int> bfsQueue({network_.sink()});
        std::vector<bool> visited(size_, false);
        visited.at(network_.sink()) = true;
        while (!bfsQueue.empty())
        {
            int vertex(bfsQueue.front());
            bfsQueue.pop();
            for (Network::edge_t& edge : network_.vertexBackEdgeList(vertex))
                if (edge.residualCapacity() > 0 && slice_.at(edge.goThroughEdge(vertex)) == slice_.at(vertex) - 1)
                {
                    visited.at(edge.goThroughEdge(vertex)) = true;
                    bfsQueue.push(edge.goThroughEdge(vertex));
                }
        }
        for (int vertex(0); vertex < size_; ++vertex)
            if (!visited.at(vertex))
                markInvalidVertex(vertex);
    }

    bool isValidEdge(const Network::edge_t& edge)
    {
        return edge.residualCapacity() > 0 && slice_.at(edge.finishVertex) == slice_.at(edge.startVertex) + 1;
    }

    void prepareIteration()
    {
        vertexStates_.assign(size_, VertexState::Valid);

        buildSlices();
        validateSlices();

        outcomePhi_.assign(size_, 0);
        incomePhi_.assign(size_, 0);

        for (int vertex(0); vertex < size_; ++vertex)
        {
            edgeLists_.at(vertex) = network_.vertexEdgeList(vertex);
            backEdgeLists_.at(vertex) = network_.vertexBackEdgeList(vertex);
            if (slice_.at(vertex) == size_)
            {
                markInvalidVertex(vertex);
                continue;
            }
            for (Network::edge_t& edge : network_.vertexEdgeList(vertex))
                if (isValidEdge(edge))
                    outcomePhi_.at(vertex) += edge.residualCapacity();
            for (Network::edge_t& edge : network_.vertexBackEdgeList(vertex))
                if (isValidEdge(edge))
                    incomePhi_.at(vertex) += edge.residualCapacity();
        }

        outcomePhi_.at(network_.sink()) = Network::Inf;
        incomePhi_.at(network_.source()) = Network::Inf;

        deleteInvalidVertexes();
    }

    int findVertexWithMinimalPhi()
    {
        if (vertexStates_.at(network_.source()) != VertexState::Valid)
            return Network::None;
        int bestVertex(Network::None);
        for (int vertex(0); vertex < size_; ++vertex)
            if (vertexStates_.at(vertex) == VertexState::Valid &&
                    (bestVertex == Network::None || phi(bestVertex) > phi(vertex))
            )
                bestVertex = vertex;
        return bestVertex;
    }

    int maxPossibleFlowThroughEdge(int currentVertex, Network::edge_t& edge)
    {
        int nextVertex(edge.goThroughEdge(currentVertex));
        return std::min(edge.residualCapacity(), phi(nextVertex));
    }

    template <typename EdgeIterator>
    void pushFlow(int referencedVertex, int finishVertex, int flow,
            std::vector<Network::view_t<EdgeIterator>>& edgeLists,
            std::vector<int>& firstPhi,
            std::vector<int>& secondPhi)
    {
        addedFlow_.at(referencedVertex) = flow;
        std::queue<int> bfsQueue({referencedVertex});
        while (!bfsQueue.empty())
        {
            int vertex(bfsQueue.front());
            bfsQueue.pop();
            secondPhi.at(vertex) -= addedFlow_.at(vertex);
            if (vertex != finishVertex)
                while (addedFlow_.at(vertex) > 0)
                {
                    Network::edge_t& edge = *edgeLists.at(vertex).begin();
                    int nextVertex(edge.goThroughEdge(vertex));
                    int currentFlow(std::min(addedFlow_.at(vertex), maxPossibleFlowThroughEdge(vertex, edge)));
                    if (vertexStates_.at(nextVertex) != VertexState::Valid
                        || !isValidEdge(edge)
                        || currentFlow == 0)
                    {
                        edgeLists.at(vertex).popLastEdge();
                        continue;
                    }
                    if (addedFlow_.at(nextVertex) == 0)
                        bfsQueue.push(nextVertex);
                    addedFlow_.at(nextVertex) += currentFlow;
                    edgeLists.at(vertex).begin().pushFlow(currentFlow);
                    firstPhi.at(nextVertex) -= currentFlow;
                    addedFlow_.at(vertex) -= currentFlow;
                }
            addedFlow_.at(vertex) = 0;
            if (phi(vertex) == 0)
                markInvalidVertex(vertex);
        }
    }

    void doIteration()
    {
        for (int flowPushingIteration(0); flowPushingIteration < size_; ++flowPushingIteration)
        {
            addedFlow_.assign(size_, 0);
            int referencedVertex(findVertexWithMinimalPhi());
            if (referencedVertex == Network::None)
                return;
            int flow(phi(referencedVertex));
            result_ += flow;
            pushFlow(referencedVertex, network_.sink(), flow, edgeLists_, incomePhi_, outcomePhi_);
            pushFlow(referencedVertex, network_.source(), flow, backEdgeLists_, outcomePhi_, incomePhi_);
            deleteInvalidVertexes();
        }
    }
public:
    void run()
    {
        prepare();
        for (int iteration(0); iteration <= size_; ++iteration)
        {
            prepareIteration();
            doIteration();
        }
    }
};

struct InputData
{
    size_t N;
    std::vector<int> costs;
    std::vector<std::vector<int>> depends;
    static InputData read(std::istream& in)
    {
        size_t N;
        in >> N;
        std::vector<int> costs(N + 1);
        for (size_t theme(1); theme <= N; ++theme)
            in >> costs.at(theme);
        std::vector<std::vector<int>> depends(1);
        depends.reserve(N + 1);
        for (size_t theme(1); theme <= N; ++theme)
        {
            size_t sizeOfDepends;
            in >> sizeOfDepends;
            depends.push_back(std::vector<int>(sizeOfDepends));
            for (int& dependingTheme : depends.back())
                in >> dependingTheme;
        }
        return {N, costs, depends};
    }
};

void solution(InputData input, std::ostream& out)
{
    Network graph(input.N + 2, 0, input.N + 1);
    int costsSum(0);
    for (int theme(1); theme <= input.N; ++theme)
    {
        if (input.costs.at(theme) >= 0)
        {
            costsSum += input.costs.at(theme);
            graph.insertEdge(graph.source(), theme, input.costs.at(theme));
        } else graph.insertEdge(theme, graph.sink(), -input.costs.at(theme));
        for (int depend : input.depends.at(theme))
            graph.insertEdge(theme, depend, Network::Inf);
    }
    MalhotraKumarMaheshwari algorithm;
    algorithm.loadNetwork(std::move(graph));
    algorithm.reset();
    algorithm.run();
    algorithm.storeNetwork(graph);
    out << costsSum - algorithm.result() << std::endl;
}

int main(int argc, char** argv)
{
    std::ios_base::sync_with_stdio(false);
    solution(InputData::read(std::cin), std::cout);
}
