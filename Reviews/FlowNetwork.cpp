#include <vector>
#include <queue>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <memory>

/**
 * 0 - source
 * 1 - sink
 */
class Network
{
public:
    constexpr static int None = -1;
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
    std::vector<int> phi_;
    std::vector<int> level_;
    std::vector<Network::view_t<Network::BackEdgeIterator>> incomeEdges_;
    std::vector<Network::view_t<Network::EdgeIterator>> outcomeEdges_;
    std::vector<bool> isDeletedVertex_;

    void prepare()
    {
        phi_.resize(size_);
        level_.resize(size_);
        isDeletedVertex_.resize(size_);
        incomeEdges_.resize(size_);
        outcomeEdges_.resize(size_);
    }

    void buildSlices()
    {
        level_.assign(size_, std::numeric_limits<int>::max());

        std::queue<int> bfsQueue;
        level_.at(network_.source()) = 0;
        bfsQueue.push(network_.source());

        while (!bfsQueue.empty())
        {
            int currentVertex(bfsQueue.front());
            bfsQueue.pop();
//            auto edgeList = network_.vertexEdgeList(currentVertex);
//            for (auto edge(edgeList.begin()); edge != edgeList.end(); ++edge)
//                if (edge.isStraight() && level_.at(edge->finishVertex) > level_.at(currentVertex) + 1) {
//                    level_.at(edge->finishVertex) = level_.at(currentVertex) + 1;
//                    bfsQueue.push(edge->finishVertex);
//                }
            for (const auto& edge : network_.vertexEdgeList(currentVertex))
                if (edge.residualCapacity() > 0 && level_.at(currentVertex) + 1 < level_.at(edge.finishVertex))
                {
                    level_.at(edge.finishVertex) = level_.at(currentVertex) + 1;
                    bfsQueue.push(edge.finishVertex);
                }
        }
    }

    bool isValidEdge(const Network::edge_t& edge)
    {
        return level_.at(edge.finishVertex) == level_.at(edge.startVertex) + 1
                && !isDeletedVertex_.at(edge.startVertex)
                && !isDeletedVertex_.at(edge.finishVertex);
    }

    void prepareIteration()
    {
        for (int vertex(0); vertex != size_; ++vertex)
        {
            incomeEdges_.at(vertex) = network_.vertexBackEdgeList(vertex);
            outcomeEdges_.at(vertex) = network_.vertexEdgeList(vertex);
        }

        buildSlices();

//        for (int vertex(0); vertex < size_; ++vertex)
//            if (level_.at(vertex) >= size_)
//                isDeletedVertex_.at(vertex) = true;
        isDeletedVertex_.assign(size_, false);
        phi_.assign(size_, 0);
        for (int vertex(0); vertex != size_; ++vertex)
            if (vertex == network_.source())
            {
                for (const auto& edge : outcomeEdges_.at(vertex))
                    if (isValidEdge(edge))
                        phi_.at(vertex) += edge.residualCapacity();
            }
            else if (vertex == network_.sink())
            {
                for (const auto& edge : incomeEdges_.at(vertex))
                    if (isValidEdge(edge))
                        phi_.at(vertex) += edge.residualCapacity();
            }
            else
            {
                int incomeResidualCapacity(0), outcomeResidualCapacity(0);
                for (const auto& edge : incomeEdges_.at(vertex))
                    if (isValidEdge(edge))
                        incomeResidualCapacity += edge.residualCapacity();
                for (const auto& edge : outcomeEdges_.at(vertex))
                    if (isValidEdge(edge))
                        outcomeResidualCapacity += edge.residualCapacity();
                phi_.at(vertex) = std::min(incomeResidualCapacity, outcomeResidualCapacity);
            }
    }

    int findVertexWithMinimalPhi()
    {
        int bestVertex(0);
        for (int vertex(0); vertex < size_; ++vertex)
            if (phi_.at(vertex) == 0)
                isDeletedVertex_.at(vertex) = true;
            else if (phi_.at(bestVertex) > phi_.at(vertex))
                bestVertex = vertex;
        return bestVertex;
    }

    template <class EdgeIteratorType>
    void pushFlow(int startVertex, int flow, int finishVertex,
            std::vector<Network::view_t<EdgeIteratorType>>& edgeListForVertex)
    {
        std::queue<int> vertexToProcess;
        static std::vector<int> addedFlow;

        addedFlow.resize(size_);
        addedFlow.assign(size_, 0);
        addedFlow.at(startVertex) = flow;
        vertexToProcess.push(startVertex);

        while (!vertexToProcess.empty())
        {
            int currentVertex(vertexToProcess.front());
            vertexToProcess.pop();
            auto& edgeList(edgeListForVertex.at(currentVertex));

            while (addedFlow.at(currentVertex) > 0 && !edgeList.empty())
            {
                if (!isValidEdge(*edgeList.begin())
                    || edgeList.begin()->residualCapacity() == 0)
                {
                    edgeList.popLastEdge();
                    continue;
                }
                int nextVertex = edgeList.begin()->goThroughEdge(currentVertex);
                int currentFlow(std::min({
                                                 addedFlow.at(currentVertex),
                                                 edgeList.begin()->residualCapacity(),
                                                 phi_.at(nextVertex) - addedFlow.at(nextVertex)
                                         }));
                if (currentFlow == 0)
                {
                    edgeList.popLastEdge();
                    continue;
                }
                edgeList.begin().pushFlow(currentFlow);
                addedFlow.at(currentVertex) -= currentFlow;
                if (addedFlow.at(nextVertex) == 0)
                    vertexToProcess.push(nextVertex);
                addedFlow.at(nextVertex) += currentFlow;
            }

            if ((phi_.at(currentVertex) -= addedFlow.at(currentVertex)) == 0)
                if (currentVertex != startVertex)
                    isDeletedVertex_.at(currentVertex) = true;
        }

        ///WARNING We mustn't change phi(startVertex)!
        phi_.at(startVertex) = flow;
    }

    void doIteration()
    {
        for (int pushFlowIteration(0); pushFlowIteration < size_ && std::min(phi_.at(network_.source()), phi_.at(network_.sink())) > 0; ++pushFlowIteration)
        {
            int bestVertex(findVertexWithMinimalPhi());
            if (phi_.at(bestVertex) == 0)
                break;
            result_ += phi_.at(bestVertex);
            pushFlow(bestVertex, phi_.at(bestVertex), network_.sink(), outcomeEdges_);
            pushFlow(bestVertex, phi_.at(bestVertex), network_.source(), incomeEdges_);
            phi_.at(bestVertex) = 0;
            isDeletedVertex_.at(bestVertex) = true;
        }
    }
public:
    MalhotraKumarMaheshwari() = default;

    void run() final
    {
        reset();
        prepare();

        for (size_t iteration(0); iteration < size_; ++iteration) // |V|
        {
            prepareIteration(); // |V| + |E|
            doIteration(); // |V| * |V| + |E|
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
    const int inf(5000);
    for (int theme(1); theme <= input.N; ++theme)
    {
        if (input.costs.at(theme) >= 0)
        {
            costsSum += input.costs.at(theme);
            graph.insertEdge(graph.source(), theme, input.costs.at(theme));
        } else graph.insertEdge(theme, graph.sink(), -input.costs.at(theme));
        for (int depend : input.depends.at(theme))
            graph.insertEdge(theme, depend, inf);
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
