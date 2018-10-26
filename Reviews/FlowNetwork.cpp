#include <vector>
#include <queue>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <memory>
#include <functional>
#include <list>
#include <cassert>

class Network
{
public:
    constexpr static int NONE = -1;
    constexpr static int INF = 1000000;
    class edge_t
    {
    private:
        int startVertex_ = NONE;
        int finishVertex_ = NONE;
        int capacity_ = 0;
        int flow_ = 0;

        void clear() { flow_ = 0; }
    public:
        friend Network;

        edge_t(int startVertex = NONE, int finishVertex = NONE, int capacity = NONE, int flow = 0)
            : startVertex_(startVertex)
            , finishVertex_(finishVertex)
            , capacity_(capacity)
            , flow_(flow)
        {}
        int startVertex() const { return startVertex_; }
        int finishVertex() const { return finishVertex_; }
        int capacity() const { return capacity_; }
        int& flow() { return flow_; }
        int flow() const { return flow_; }

        int residualCapacity() const
        {
            return capacity_ - flow_;
        }

        int goThroughEdge(int fromVertex) const { return startVertex_ == fromVertex ? finishVertex_ : startVertex_; }
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
        : lastEdge_(N, NONE)
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
        int                     edgeId_ = NONE;

        EdgeIteratorImpl(
                std::vector<edge_t>* edges = nullptr,
                std::vector<int>* previousEdge = nullptr,
                int edgeId = NONE)
            : edges_(edges)
            , previousEdge_(previousEdge)
            , edgeId_(edgeId)
        {}

        edge_t& getBackEdgeImpl() const
        {
            if constexpr (isBackEdgeIterator)
                return edges_->at(edgeId_);
            else
                return edges_->at(edgeId_ ^ 1);
        }

        edge_t& getEdgeImpl() const
        {
            if constexpr (isBackEdgeIterator)
                return edges_->at(edgeId_ ^ 1);
            else
                return edges_->at(edgeId_);
        }
    public:
        friend Network;

        int getEdgeId() const
        {
            if constexpr (isBackEdgeIterator)
                return edgeId_ ^ 1;
            else
                return edgeId_;
        }

        bool isStraight() { return getEdgeId() & 1; }

        const edge_t& getEdge() const { return getEdgeImpl(); };

        const edge_t& getBackEdge() const { return getBackEdgeImpl(); }

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

        const edge_t& operator*() const { return getEdgeImpl(); }
        const edge_t* operator->() const { return &getEdgeImpl(); }

        explicit operator bool() const { return edgeId_ != NONE; }

        bool operator==(const EdgeIteratorImpl& other) const
        {
            if (!operator bool() && !other)
                return true;
            return edges_ == other.edges_ && edgeId_ == other.edgeId_;
        }

        bool operator!=(const EdgeIteratorImpl& other) const { return !(operator==(other)); }

        void pushFlow(int flow) const
        {
            getEdgeImpl().flow() += flow;
            getBackEdgeImpl().flow() -= flow;
        }
    };
public:
    using EdgeIterator = EdgeIteratorImpl<false>;
    using BackEdgeIterator = EdgeIteratorImpl<true>;

    auto begin() { return edges_.begin(); }
    auto end() { return edges_.end(); }

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
            edge.clear();
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
    bool networkLoaded_ = false;

    bool checkNetwork() { return networkLoaded_; }
public:
    FlowFindingAlgorithm() = default;

    int result() { return result_; }
    void reset() { network_.clear(); result_ = 0; }

    void loadNetwork(Network&& network) { network_ = std::move(network); size_ = network_.size(); networkLoaded_ = true; }
    void storeNetwork(Network& network) { network = std::move(network_); size_ = 0; networkLoaded_ = false; }

    virtual bool run() = 0;
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
    std::vector<int> verticesToDelete_;

    int phi(int vertex) const { return std::min(incomePhi_.at(vertex), outcomePhi_.at(vertex)); }

    void prepare()
    {
        incomePhi_.resize(size_);
        outcomePhi_.resize(size_);
        slice_.resize(size_);
        edgeLists_.resize(size_);
        backEdgeLists_.resize(size_);
        vertexStates_.resize(size_);
        verticesToDelete_.clear();
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
            for (const Network::edge_t& edge : network_.vertexEdgeList(vertex))
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
            verticesToDelete_.push_back(vertex);
            vertexStates_.at(vertex) = VertexState::CandidateToDelete;
        }
    }

    void deleteInvalidVertices()
    {
        while (!verticesToDelete_.empty())
        {
            int vertex(verticesToDelete_.back());
            verticesToDelete_.pop_back();
            vertexStates_.at(vertex) = VertexState::Deleted;
            for (const Network::edge_t& edge : edgeLists_.at(vertex))
                if (isValidEdge(edge))
                {
                    int nextVertex(edge.goThroughEdge(vertex));
                    if ((incomePhi_.at(nextVertex) -= edge.residualCapacity()) == 0)
                        markInvalidVertex(nextVertex);
                }
            for (const Network::edge_t& edge : backEdgeLists_.at(vertex))
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
            for (const Network::edge_t& edge : network_.vertexBackEdgeList(vertex))
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

    bool isValidEdge(const Network::edge_t& edge) const
    {
        return edge.residualCapacity() > 0 && slice_.at(edge.finishVertex()) == slice_.at(edge.startVertex()) + 1;
    }
    
    template <class EdgeIterator>
    void calcPartialPhi(int& partialPhi, Network::view_t<EdgeIterator> edges) const
    {
        for (const Network::edge_t& edge : edges)
            if (isValidEdge(edge))
                partialPhi += edge.residualCapacity();
    }
    
    void initializePhi()
    {
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
            calcPartialPhi(outcomePhi_.at(vertex), network_.vertexEdgeList(vertex));
            calcPartialPhi(incomePhi_.at(vertex), network_.vertexBackEdgeList(vertex));
        }

        outcomePhi_.at(network_.sink()) = Network::INF;
        incomePhi_.at(network_.source()) = Network::INF;
    }

    bool prepareIteration()
    {
        vertexStates_.assign(size_, VertexState::Valid);

        buildSlices();
        validateSlices();

        initializePhi();

        deleteInvalidVertices();

        return vertexStates_.at(network_.source()) == VertexState::Valid
                && vertexStates_.at(network_.sink()) == VertexState::Valid;
    }

    int findVertexWithMinimalPhi()
    {
        if (vertexStates_.at(network_.source()) != VertexState::Valid)
            return Network::NONE;
        int bestVertex(Network::NONE);
        for (int vertex(0); vertex < size_; ++vertex)
            if (vertexStates_.at(vertex) == VertexState::Valid &&
                    (bestVertex == Network::NONE || phi(bestVertex) > phi(vertex))
            )
                bestVertex = vertex;
        return bestVertex;
    }

    int maxPossibleFlowThroughEdge(int currentVertex, const Network::edge_t& edge) const
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
                    const Network::edge_t& edge = *edgeLists.at(vertex).begin();
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
            if (referencedVertex == Network::NONE)
                return;
            int flow(phi(referencedVertex));
            result_ += flow;
            pushFlow(referencedVertex, network_.sink(), flow, edgeLists_, incomePhi_, outcomePhi_);
            pushFlow(referencedVertex, network_.source(), flow, backEdgeLists_, outcomePhi_, incomePhi_);
            deleteInvalidVertices();
        }
    }
public:
    bool run() final
    {
        if (!checkNetwork())
            return false;
        reset();
        prepare();
        for (int iteration(0); iteration <= size_; ++iteration)
        {
            if (!prepareIteration())
                break;
            doIteration();
        }
        return true;
    }
};

class PreflowPushAlgorithm : public FlowFindingAlgorithm
{
private:
    std::vector<int> height_, overage_;
    std::vector<Network::view_t<Network::EdgeIterator>> edgeLists_;
    
    void pushFlowImpl(Network::EdgeIterator edge, int flow)
    {

        edge.pushFlow(flow);
        overage_.at(edge->startVertex()) -= flow;
        overage_.at(edge->finishVertex()) += flow;
    }

    void prepare()
    {
        height_.resize(size_);
        height_.assign(size_, 0);
        height_.at(network_.source()) = size_;

        overage_.resize(size_);
        overage_.assign(size_, 0);

        for (
                auto edge = network_.vertexEdgeList(network_.source()).begin();
                edge != network_.vertexEdgeList(network_.source()).end();
                ++edge
        )
        {
            pushFlowImpl(edge, edge->capacity());
        }

        edgeLists_.clear();
        edgeLists_.reserve(size_);
        for (int vertex(0); vertex < size_; ++vertex)
            edgeLists_.push_back(network_.vertexEdgeList(vertex));
    }

    void push(Network::EdgeIterator edge)
    {
        int flow(std::min(overage_.at(edge->startVertex()), edge->residualCapacity()));
        pushFlowImpl(edge, flow);
    }

    void relabel(int vertex)
    {
        int newHeight(Network::INF);
        for (auto& edge : network_.vertexEdgeList(vertex))
            if (edge.residualCapacity() > 0)
                newHeight = std::min(newHeight, height_.at(edge.finishVertex()));
        height_.at(vertex) = newHeight + 1;
    }

    bool discharge(int vertex)
    {
        if (overage_.at(vertex) <= 0)
            return false;
        while (overage_.at(vertex) > 0)
        {
            auto& edges = edgeLists_.at(vertex);
            if (edges.empty())
            {
                edges = network_.vertexEdgeList(vertex);
                relabel(vertex);
            }
            else
            {
                if (edges.begin()->residualCapacity() > 0
                && height_.at(edges.begin()->finishVertex()) + 1 == height_.at(vertex))
                    push(edges.begin());
                else
                    edges.popLastEdge();
            }
        }
        return true;
    }
public:
    bool run() final
    {
        if (!checkNetwork())
            return false;
        reset();
        prepare();
        bool canDoPushOrRelabel;
        do
        {
            canDoPushOrRelabel = false;
            for (int vertex(0); vertex < size_; ++vertex)
                if (vertex != network_.source() && vertex != network_.sink())
                    canDoPushOrRelabel |= discharge(vertex);
        } while (canDoPushOrRelabel);
        result_ = overage_.at(network_.sink());
        return true;
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

template <typename Algorithm>
int solution(const InputData& input)
{
    Network graph(input.N + 2, 0, input.N + 1);
    int costsSum(0);
    for (int theme(1); theme <= input.N; ++theme)
    {
        if (input.costs.at(theme) > 0)
        {
            costsSum += input.costs.at(theme);
            graph.insertEdge(graph.source(), theme, input.costs.at(theme));
        } else if (input.costs.at(theme) < 0) graph.insertEdge(theme, graph.sink(), -input.costs.at(theme));
        for (int depend : input.depends.at(theme))
            graph.insertEdge(theme, depend, Network::INF);
    }
    std::unique_ptr<FlowFindingAlgorithm> algorithm = std::make_unique<Algorithm>();
    algorithm->loadNetwork(std::move(graph));
    algorithm->reset();
    algorithm->run();
    algorithm->storeNetwork(graph);
    return costsSum - algorithm->result();
}

void run()
{
    auto data = InputData::read(std::cin);
    int result(solution<PreflowPushAlgorithm>(data));
    assert(result == solution<MalhotraKumarMaheshwari>(data));
    std::cout << result << std::endl;
}

int main(int argc, char** argv)
{
    std::ios_base::sync_with_stdio(false);
    run();
}
