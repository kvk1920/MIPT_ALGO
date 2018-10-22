#include <vector>

template <typename capacity_t>
class FlowNetwork
{
public:
    using id_t = int;

    static const id_t None = -1;

    struct edge_t
    {
        id_t            startVertex = None;
        id_t            finishVertex = None;
        capacity_t      capacity = 0;
        capacity_t      flow = 0;
        capacity_t      residualCapacity() const;
    };
private:
    std::vector<edge_t> edges_;
    std::vector<id_t>   previous_edge_;
    std::vector<id_t>   last_edge_;

    void baseInsertEdge(id_t fromVertex, id_t toVertex, capacity_t capacity);
public:
    FlowNetwork(size_t n);
};

template <typename capacity_t> inline capacity_t
FlowNetwork<capacity_t>::edge_t::residualCapacity() const
{
    return capacity - flow;
}

template <typename capacity_t> inline void
FlowNetwork<capacity_t>::baseInsertEdge(
        id_t fromVertex,
        id_t toVertex,
        capacity_t capacity)
{
    previous_edge_.push_back(last_edge_[fromVertex]);
    last_edge_[fromVertex] = edges_.size();
    edges_.push_back({fromVertex, toVertex, capacity});
}

template <typename capacity_t> inline
FlowNetwork<capacity_t>::FlowNetwork(size_t n)
    : last_edge_(n, None)
{}
