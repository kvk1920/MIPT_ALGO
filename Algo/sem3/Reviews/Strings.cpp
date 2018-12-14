#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

inline void Assert(bool expr) {
    if (!expr) {
        volatile int x = 0;
        while (true) ++x;
    }
}

template <typename TChar>
class SubstringMachine {
  public:
    using TString = std::basic_string<TChar>;

    class RightContextIterator;

    virtual ~SubstringMachine() = default;

    RightContextIterator GetRightContextIterator() const {
        return RightContextIterator(GetVertices());
    }

  protected:
    struct IVertex;

    virtual std::shared_ptr<const std::vector<std::shared_ptr<const IVertex>>> GetVertices() const = 0;
};

template <typename TChar>
struct SubstringMachine<TChar>::IVertex {
    virtual ~IVertex() = default;
    virtual SubstringMachine<TChar>::TString GetString() const = 0;

    virtual size_t GetMaximalLength() const = 0;

    virtual size_t GetNumOfOccurrences() const = 0;
};

template <typename TChar>
class SubstringMachine<TChar>::RightContextIterator {
  public:
    explicit RightContextIterator(
        std::shared_ptr<const std::vector<std::shared_ptr<const SubstringMachine<TChar>::IVertex>>> vertices
        )
            : vertices_(std::move(vertices))
            , index_(0)
    {}

    RightContextIterator() = default;

    RightContextIterator& operator=(const RightContextIterator&) = default;
    RightContextIterator(const RightContextIterator&) = default;
    RightContextIterator(RightContextIterator&&) noexcept = default;

    bool Valid() const noexcept {
        return vertices_ && index_ < std::size(*vertices_);
    }

    RightContextIterator Next() const noexcept {
        Assert(Valid());
        RightContextIterator next(*this);
        ++next.index_;
        return next;
    }

    SubstringMachine<TChar>::TString GetStateString() const {
        Assert(Valid());
        return vertices_->at(index_)->GetString();
    }

    size_t GetMaximalLength() const {
        Assert(Valid());
        return vertices_->at(index_)->GetMaximalLength();
    }

    size_t GetNumOfOccurrences() const {
        Assert(Valid());
        return vertices_->at(index_)->GetNumOfOccurrences();
    }

  private:
    std::shared_ptr<const std::vector<std::shared_ptr<const SubstringMachine<TChar>::IVertex>>> vertices_ = nullptr;
    size_t index_ = 0;
};

template <typename TChar>
class SuffixMachine : public SubstringMachine<TChar> {
  public:
    using TString = typename SubstringMachine<TChar>::TString;

    explicit SuffixMachine(const TString& string) : SuffixMachine(TString(string)) {}

    explicit SuffixMachine(TString&& string)
            : string_(std::move(string))
            , vertices_(std::make_shared<std::vector<std::shared_ptr<const typename SuffixMachine<TChar>::IVertex>>>())
    {
        vertices_->reserve(std::size(string_) * 2);
        Init();
        Build();
        Finally();
    }

  private:
    struct Vertex : public SubstringMachine<TChar>::IVertex {
        TString GetString() const final {
            TString string;
            string.clear();

            string.reserve(length);
            string.push_back(edge_char);

            for (auto vertex(parent.lock()); vertex->parent.lock(); vertex = vertex->parent.lock()) {
                string.push_back(vertex->edge_char);
            }

            std::reverse(std::begin(string), std::end(string));
            return string;
        }

        size_t GetMaximalLength() const final {
            return length;
        }

        size_t GetNumOfOccurrences() const final {
            return num_of_occurrences;
        }

        TChar edge_char;
        std::weak_ptr<Vertex> suffix_link;
        std::weak_ptr<Vertex> parent;
        size_t length = 0;
        size_t num_of_occurrences = 0;
        std::map<TChar, std::shared_ptr<Vertex>> next;
        bool is_terminal = false;
    };

    void Init() {
        last_ = root_ = std::make_shared<Vertex>();
        root_->suffix_link = root_;
    }

    void Build() {
        for (auto c : string_) {
            auto v(std::make_shared<Vertex>());
            v->length = last_->length + 1;
            v->edge_char = c;
            v->parent = last_;
            auto p(last_);
            last_ = v;

            for (; !p->next.count(c); p = p->suffix_link.lock()) {
                p->next.insert({c, v});
            }

            if (p->next.at(c) == v) {
                v->suffix_link = root_;
                continue;
            }

            auto q(p->next.at(c));
            if (q->length == p->length + 1) {
                v->suffix_link = q;
                continue;
            }

            auto clone(std::make_shared<Vertex>());
            clone->suffix_link = q->suffix_link;
            clone->length = p->length + 1;
            clone->parent = p;
            clone->edge_char = c;
            clone->next = q->next;
            v->suffix_link = q->suffix_link = clone;
            for (; p->next.at(c) == q; p = p->suffix_link.lock()) {
                p->next.at(c) = clone;
            }
        }
    }

    void Finally() {
        root_->is_terminal = true;
        for (auto v(last_); v != root_; v = v->suffix_link.lock()) {
            v->is_terminal = true;
        }
        std::set<std::shared_ptr<Vertex>> seen_vertices;
        ProcessVertex(root_, seen_vertices);
    }

    void ProcessVertex(std::shared_ptr<Vertex> vertex, std::set<std::shared_ptr<Vertex>>& seen_vertices) {
        if (seen_vertices.count(vertex)) {
            return;
        }
        seen_vertices.insert(vertex);
        vertex->num_of_occurrences = vertex->is_terminal ? 1 : 0;
        for (const auto& [next_char, next_vertex] : vertex->next) {
            ProcessVertex(next_vertex, seen_vertices);
            vertex->num_of_occurrences += next_vertex->num_of_occurrences;
        }
        if (vertex != root_) {
            vertices_->push_back(std::dynamic_pointer_cast<typename SubstringMachine<TChar>::IVertex>(vertex));
        }
    }

    std::shared_ptr<const std::vector<std::shared_ptr<const typename SubstringMachine<TChar>::IVertex>>> GetVertices() const final {
        return vertices_;
    }

    TString string_;
    std::shared_ptr<std::vector<std::shared_ptr<const typename SubstringMachine<TChar>::IVertex>>> vertices_;
    std::shared_ptr<Vertex> root_;
    std::shared_ptr<Vertex> last_;
};

template <typename TChar>
class SuffixTree : public SubstringMachine<TChar> {
  public:
    using TString = typename SubstringMachine<TChar>::TString;

    explicit SuffixTree(const TString& string) : SuffixTree(TString(string)) {}

    explicit SuffixTree(TString&& string)
            : INFINITY(std::size(string))
            , string_(std::make_shared<TString>(std::move(string)))
            , vertices_(std::make_shared<std::vector<std::shared_ptr<const typename SubstringMachine<TChar>::IVertex>>>())
    {
        vertices_->reserve(std::size(string) * 2);
        Init();
        Build();
        Finally();
    }

  private:
    struct Vertex : public SubstringMachine<TChar>::IVertex {
        TString GetString() const final {
            std::vector<TString> strings;
            TString result;
            result.clear();

            strings.push_back(string->substr(left_bound, right_bound - left_bound));
            for (auto v(parent.lock()); v->parent.lock(); v = v->parent.lock()) {
                strings.push_back(string->substr(v->left_bound, v->right_bound - v->left_bound));
            }

            while (!strings.empty()) {
                result += strings.back();
                strings.pop_back();
            }

            return result;
        }

        size_t GetMaximalLength() const final {
            return distance_from_root;
        }

        size_t GetNumOfOccurrences() const final {
            return num_of_occurrences;
        }

        TChar GetChar(size_t distance_from_this) const {
            return string->at(right_bound - distance_from_this);
        }

        size_t Length() const {
            return right_bound - left_bound;
        }

        TChar FirstChar() const {
            return string->at(left_bound);
        }

        bool IsLeaf() const {
            return children.empty();
        }

        std::map<TChar, std::shared_ptr<Vertex>> children;
        bool is_terminal = false;
        size_t num_of_occurrences = 0;
        size_t distance_from_root = 0;
        size_t left_bound = 0, right_bound = 0;
        std::shared_ptr<const TString> string;
        std::weak_ptr<Vertex> parent;
        std::weak_ptr<Vertex> suffix_link;
    };

    /*std::shared_ptr<Vertex> GetLink(std::shared_ptr<Vertex> vertex) {
        if (auto link = vertex->suffix_link.lock()) {
            return link;
        } else {
            BuildSuffixLink(vertex);
            return vertex->suffix_link.lock();
        }
    }*/

    void BuildSuffixLink(std::shared_ptr<Vertex> vertex) {
        if (auto p(vertex->parent.lock()); p == root_) {
            vertex->suffix_link = SplitEdge(Position{root_}.Go(vertex->left_bound + 1, vertex->right_bound));
        } else {
            vertex->suffix_link = SplitEdge(Position{p->suffix_link.lock()}.Go(vertex->left_bound, vertex->right_bound));
        }
    }

    void Init() {
        root_ = std::make_shared<Vertex>();
        root_->string = string_;
        last_not_leaf_ = { root_, 0 };
    }

    void MakeLeaf(std::shared_ptr<Vertex> vertex, size_t position) {
        auto leaf(std::make_shared<Vertex>());

        leaf->left_bound = position;
        leaf->right_bound = INFINITY;
        leaf->parent = vertex;
        leaf->is_terminal = true;

        leaf->string = string_;

        vertex->children.insert({ leaf->FirstChar(), leaf });
    }

    void Build() {
        for (size_t i(0); i < std::size(*string_); ++i) {
            auto c(string_->at(i));
            while (true) {
                if (last_not_leaf_.CanGo(c)) {
                    last_not_leaf_ = last_not_leaf_.OneStepDown(c);
                    break;
                }
                auto vertex(SplitEdge(last_not_leaf_));
                MakeLeaf(vertex, i);
                if (vertex == root_) {
                    break;
                }
                last_not_leaf_ = {vertex->suffix_link.lock()};
            }
        }
    }

    void ProcessVertex(std::shared_ptr<Vertex> vertex) {
        if (vertex == root_) {
            vertex->distance_from_root = 0;
        } else {
            vertex->distance_from_root = vertex->Length() + vertex->parent.lock()->distance_from_root;
            vertices_->push_back(std::dynamic_pointer_cast<const typename SubstringMachine<TChar>::IVertex>(vertex));
        }
        vertex->num_of_occurrences = vertex->is_terminal ? 1 : 0;
        for (const auto& [next_char, child] : vertex->children) {
            ProcessVertex(child);
            vertex->num_of_occurrences += child->num_of_occurrences;
        }
    }

    void Finally() {
        for (auto v(SplitEdge(last_not_leaf_)); v != root_; v = v->suffix_link.lock()) {
            v->is_terminal = true;
        }
        ProcessVertex(root_);
    }

    std::shared_ptr<const std::vector<std::shared_ptr<const typename SubstringMachine<TChar>::IVertex>>> GetVertices() const {
        return vertices_;
    }

    struct Position {
        bool IsVertex() const {
            return distance_from_down_vertex == 0;
        }

        bool CanGo(const TChar c) const {
            if (IsVertex()) {
                return down_vertex->children.count(c) > 0;
            } else {
                return c == down_vertex->GetChar(distance_from_down_vertex);
            }
        }

        Position OneStepDown(TChar c) {
            if (IsVertex()) {
                Position result{down_vertex->children.at(c)};
                result.distance_from_down_vertex = result.down_vertex->Length() - 1;
                return result;
            } else {
                Position result(*this);
                --result.distance_from_down_vertex;
                return result;
            }
        }

        Position Go(size_t l, const size_t r) {
            const auto& string = *(down_vertex->string);
            Position result(*this);
            while (l < r) {
                if (result.IsVertex()) {
                    result.down_vertex = result.down_vertex->children.at(string[l]);
                    result.distance_from_down_vertex = result.down_vertex->Length();
                } else if (r - l > result.distance_from_down_vertex) {
                    l += result.distance_from_down_vertex;
                    result.distance_from_down_vertex = 0;
                } else {
                    result.distance_from_down_vertex -= r - l;
                    l = r;
                }
            }
            return result;
        }

        std::shared_ptr<Vertex> down_vertex;
        size_t distance_from_down_vertex = 0;
    };

    std::shared_ptr<Vertex> SplitEdge(Position position) {
        if (position.IsVertex()) {
            return position.down_vertex;
        }
        auto u(position.down_vertex->parent.lock());
        auto v(position.down_vertex);
        auto new_v(std::make_shared<Vertex>());
        new_v->string = string_;
        u->children.at(v->FirstChar()) = new_v;
        new_v->parent = u;
        new_v->left_bound = v->left_bound;
        new_v->right_bound = v->left_bound = v->right_bound - position.distance_from_down_vertex;
        v->parent = new_v;
        new_v->children.insert({ v->FirstChar(), v });

        BuildSuffixLink(new_v);

        return new_v;
    }

    const size_t INFINITY;
    std::shared_ptr<Vertex> root_;
    Position last_not_leaf_;
    std::shared_ptr<TString> string_;
    std::shared_ptr<std::vector<std::shared_ptr<const typename SubstringMachine<TChar>::IVertex>>> vertices_;
};

inline std::basic_string<int> ReadIntString(size_t length, std::istream& in) {
    int element;
    std::basic_string<int> string;
    while (length--) {
        in >> element;
        string.push_back(element);
    }
    return string;
}

inline void WriteIntString(const std::basic_string<int>& string, std::ostream& out) {
    for (size_t i(0); i + 1 < std::size(string); ++i) {
        out << string[i] << ' ';
    }
    if (!string.empty()) {
        out << string.back();
    }
}

int main() {
    using namespace std;

    ios_base::sync_with_stdio(false);
    int n, m;
    cin >> n >> m;
    auto s(ReadIntString(n, cin));
    std::unique_ptr<SubstringMachine<int>> machine(new SuffixTree<int>(s));
    SubstringMachine<int>::RightContextIterator best_it;
    long long best_val(0);
    for (SubstringMachine<int>::RightContextIterator it(machine->GetRightContextIterator()); it.Valid(); it = it.Next()) {
        if (it.GetMaximalLength() * it.GetNumOfOccurrences() > best_val) {
            best_val = int64_t(it.GetMaximalLength()) * it.GetNumOfOccurrences();
            best_it = it;
        }
        //WriteIntString(it.GetStateString(), cerr);
        //cerr << endl << it.GetMaximalLength() << ' ' << it.GetNumOfOccurrences() << endl;
    }
    cout << best_val << '\n';
    auto ans(best_it.GetStateString());
    cout << size(ans) << '\n';
    WriteIntString(ans, cout);
}
