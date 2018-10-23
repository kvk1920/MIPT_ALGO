#include <vector>

template <class Container>
struct ContainerView
{
private:
    Container*    container_;
public:
    ContainerView(Container* container);
    auto begin() const;
    auto end() const;
    auto begin();
    auto end();
};

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

    auto edges() const;
    auto edges();

    template <typename T>
    class edge_iterator : public std::iterator<std::forward_iterator_tag, T>
    {
    private:
        id_t            edgeId_;
    public:
        T* operator->() const;
        T& operator*() const;
        auto& operator++();
        auto operator++(int);
    };

    template <bool is_const = false>
    struct VertexView
    {
    private:
        id_t vertexId_ = None;
    public:
        auto begin() const;
        auto end() const;
    };

     auto vertex(id_t vertexId) const;
     auto vertex(id_t vertexId);
};

template <class Container> inline
ContainerView<Container>::ContainerView(Container* container) : container_(container) {}

template <class Container> inline
auto ContainerView<Container>::begin()
{
    return container_->begin();
}

template <class Container> inline
auto ContainerView<Container>::end()
{
    return container_->end();
}

template <class Container> inline
auto ContainerView<Container>::begin() const
{
    return container_->begin();
}

template <class Container> inline
auto ContainerView<Container>::end() const
{
    return container_->end();
}

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

template <typename capacity_t> inline
auto FlowNetwork<capacity_t>::edges() const
{
    return ContainerView<const std::vector<edge_t>>(&edges_);
}

template <typename capacity_t> inline
auto FlowNetwork<capacity_t>::edges()
{
    return ContainerView<std::vector<edge_t>>(&edges_);
}
