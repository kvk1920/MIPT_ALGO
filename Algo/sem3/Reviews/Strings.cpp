#include <iostream>
#include <string>
#include <memory>
#include <vector>

template <typename TChar>
class SubstringMachine {
  public:
    using TString = std::basic_string<TChar>;

    class RightContextIterator;

    virtual ~SubstringMachine() = default;

    RightContextIterator GetRightContextIterator() const;

  protected:
    SubstringMachine(const TString& string);
    SubstringMachine(TString&& string);

  private:
    class VertexInterface;

    bool finished_;
    TString string;
    std::shared_ptr<std::vector<std::shared_ptr<VertexInterface>>> vertices_;
};

template <typename TChar>
auto SubstringMachine<TChar>::GetRightContextIterator() const -> RightContextIterator {
        return RightContextIterator(vertices_);
}

template <typename TChar>
class SubstringMachine<TChar>::RightContextIterator {
  public:
    RightContextIterator(std::shared_ptr<const std::vector<std::shared_ptr<const VertexInterface>>> vertices);

    bool Next() const noexcept;
    bool Valid() const noexcept;

    size_t MaxLength() const;
    size_t NumOfOccurences() const;
  private:
    std::shared_ptr<const std::vector<const std::shared_ptr<const VertexInterface>>> vertices_;
    size_t index_;
};


