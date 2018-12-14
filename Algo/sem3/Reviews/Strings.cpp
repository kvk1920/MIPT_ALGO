#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

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
        RightContextIterator next(*this);
        ++next.index_;
        return next;
    }

    SubstringMachine<TChar>::TString GetStateString() const {
        return vertices_->at(index_)->GetString();
    }

    size_t GetMaximalLength() const {
        return vertices_->at(index_)->GetMaximalLength();
    }

    size_t GetNumOfOccurrences() const {
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
        TString GetString() const {
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

            if (p->next[c] == v) {
                v->suffix_link = root_;
                continue;
            }

            auto q(p->next[c]);
            if (q->length == p->length + 1) {
                v->suffix_link = q;
                continue;
            }

            auto clone(std::make_shared<Vertex>());
            clone->suffix_link = q->suffix_link;
            clone->length = q->length;
            clone->parent = p;
            clone->edge_char = c;
            clone->next = q->next;
            v->suffix_link = q->suffix_link = clone;
            for (; p->next[c] == q; p = p->suffix_link.lock()) {
                p->next[c] = clone;
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
        vertices_->push_back(vertex);
    }

    std::shared_ptr<const std::vector<std::shared_ptr<const typename SubstringMachine<TChar>::IVertex>>> GetVertices() const final {
        return vertices_;
    }

    TString string_;
    std::shared_ptr<std::vector<std::shared_ptr<const typename SubstringMachine<TChar>::IVertex>>> vertices_;
    std::shared_ptr<Vertex> root_;
    std::shared_ptr<Vertex> last_;
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
    std::unique_ptr<SubstringMachine<int>> machine(new SuffixMachine<int>(s));
    SubstringMachine<int>::RightContextIterator best_it;
    long long best_val(0);
    for (SubstringMachine<int>::RightContextIterator it(machine->GetRightContextIterator()); it.Valid(); it = it.Next()) {
        if (it.GetMaximalLength() * it.GetNumOfOccurrences() > best_val) {
            best_val = (long long)it.GetMaximalLength() * it.GetNumOfOccurrences();
            best_it = it;
        }
    }
    cout << best_val << '\n';
    auto ans(best_it.GetStateString());
    cout << size(ans) << '\n';
    WriteIntString(ans, cout);
}
