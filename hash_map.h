#pragma once

#include <vector>
#include <queue>
#include <stdexcept>
#include <tuple>

// currently using: Robin Hood hashing
template<class KeyType, class ValueType, class Hash = std::hash<KeyType>>
class HashMap {
public:
    using ElementType = std::pair<KeyType, ValueType>;

    static const std::size_t DEFAULT_SIZE = 64;
    static const std::size_t MAX_LOAD_FACTOR = 80; // in percents
    static const std::size_t MAX_LOAD_FACTOR_ON_REBUILD = 50;
    static const std::size_t SIZE_INCREASING_ON_REBUILD = 4;

    template<bool isConst>
    class MapIterator {
    public:
        using ReturnElementType = std::pair<const KeyType, ValueType>;
        using ReferenceType = std::conditional_t<isConst, const ReturnElementType&, ReturnElementType&>;
        using PointerType = std::conditional_t<isConst, const ReturnElementType*, ReturnElementType*>;
        using DataType = std::conditional_t<isConst, const std::vector<ElementType>, std::vector<ElementType>>;

        MapIterator() : index_(0), data_pointer_(nullptr), element_positions_pointer_(nullptr) {
        }
        MapIterator(DataType* data, const std::vector<std::size_t>* poses, std::size_t index = 0)
                : index_(index), data_pointer_(data), element_positions_pointer_(poses) {
        }

        ReferenceType operator*() const {
            return reinterpret_cast<ReferenceType>((*data_pointer_)[GetIndex()]);
        }
        PointerType operator->() const {
            return reinterpret_cast<PointerType>(&((*data_pointer_)[GetIndex()]));
        }

        MapIterator& operator++() {
            ++index_;
            return *this;
        }
        MapIterator operator++(int) {
            auto answer = MapIterator<isConst>(data_pointer_, element_positions_pointer_, index_);
            ++index_;
            return answer;
        }

        bool operator==(const MapIterator& other) const {
            return index_ == other.index_;
        }
        bool operator!=(const MapIterator& other) const {
            return index_ != other.index_;
        }
    private:
        std::size_t GetIndex() const {
            return (*element_positions_pointer_)[index_];
        }

        std::size_t index_;

        DataType* data_pointer_;
        const std::vector<std::size_t>* element_positions_pointer_;
    };

    using iterator = MapIterator<false>;
    using const_iterator = MapIterator<true>;

    explicit HashMap(Hash hash = Hash()) : hash_(hash) {
        Init(DEFAULT_SIZE);
    }
    template<class ForwardIterator>
    HashMap(ForwardIterator begin, ForwardIterator end, Hash hash = Hash()) : hash_(hash) {
        Init(DEFAULT_SIZE);

        while (begin != end) {
            InsertElement(begin->first, begin->second);

            ++begin;
        }
    }
    HashMap(std::initializer_list<std::pair<KeyType, ValueType>> initializer_list, Hash hash = Hash()): hash_(hash) {
        Init(DEFAULT_SIZE);

        for (auto& e : initializer_list) {
            InsertElement(e.first, e.second);
        }
    }

    std::size_t size() const {
        return element_count_;
    }
    bool empty() const {
        return element_count_ == 0;
    }

    Hash hash_function() const {
        return hash_;
    }

    void insert(const ElementType& element) {
        std::size_t i = GetKeyIndex(element.first);
        if (!free_places_[i]) {
            return;
        }
        InsertElement(element.first, element.second);
    }
    void erase(const KeyType& key) {
        std::size_t i = GetKeyIndex(key);
        if (free_places_[i]) {
            return;
        }
        DeleteElementByPosition(i);
    }

    iterator begin() {
        return iterator(&data_, &element_positions_, 0);
    }
    const_iterator begin() const {
        return const_iterator(&data_, &element_positions_, 0);
    }
    iterator end() {
        return iterator(&data_, &element_positions_, element_positions_.size());
    }
    const_iterator end() const{
        return const_iterator(&data_, &element_positions_, element_positions_.size());
    }

    iterator find(const KeyType& key) {
        std::size_t i = GetKeyIndex(key);
        if (free_places_[i]) {
            return end();
        }
        return iterator(&data_, &element_positions_, index_in_element_positions_[i]);
    }
    const_iterator find(const KeyType& key) const {
        std::size_t i = GetKeyIndex(key);
        if (free_places_[i]) {
            return end();
        }
        return const_iterator(&data_, &element_positions_, index_in_element_positions_[i]);
    }

    ValueType& operator[](const KeyType& key) {
        std::size_t i = GetKeyIndex(key);
        if (free_places_[i]) {
            i = InsertElement(key, ValueType());
        }
        return data_[i].second;
    }
    const ValueType& at(const KeyType& key) const {
        std::size_t i = GetKeyIndex(key);
        if (free_places_[i]) {
            throw std::out_of_range("can't find element");
        }
        return data_[i].second;
    }

    void clear() {
        while (!element_positions_.empty()) {
            DeleteElementByIndex(0);
        }
    }

private:
    void Init(std::size_t size) {
        size_ = size;
        element_count_ = 0;

        data_ = std::vector<ElementType>(size);
        psl_ = std::vector<std::size_t>(size, 0);
        free_places_ = std::vector<bool>(size, true);
        index_in_element_positions_ = std::vector<std::size_t>(size, 0);
        element_positions_.clear();
        element_positions_.reserve((MAX_LOAD_FACTOR + 1) * size_ / 100);
    }

    std::size_t GetKeyIndex(const KeyType& key) const {
        std::size_t i = hash_(key) % size_;
        while (!free_places_[i]) {
            if (data_[i].first == key) {
                break;
            }
            else {
                ++i;
            }
            if (i == size_) {
                i = 0;
            }
        }
        return i;
    }

    void InsertByIndex(size_t i, const KeyType& key, const ValueType& value, std::size_t psl = 1) {
        if (free_places_[i]) {
            ++element_count_;
            index_in_element_positions_[i] = element_positions_.size();
            element_positions_.push_back(i);
            free_places_[i] = false;
            data_[i].first = key;
            data_[i].second = value;
            psl_[i] = psl;
        }
        else {
            data_[i].second = value;
        }
    }
    bool IsCoolLoad(std::size_t size, std::size_t load_factor = MAX_LOAD_FACTOR) const {
        return element_count_ * 100 < load_factor * size;
    }
    void Rebuild() {
        std::size_t new_size = size_;
        do {
            new_size *= SIZE_INCREASING_ON_REBUILD;
        } while (!IsCoolLoad(new_size, MAX_LOAD_FACTOR_ON_REBUILD));

        std::queue<ElementType> elements;
        for (auto i : element_positions_) {
            elements.push(std::move(data_[i]));
        }
        Init(new_size);
        while (!elements.empty()) {
            auto e = elements.front();
            elements.pop();
            InsertElement(e.first, e.second);
        }
    }
    std::size_t RobinHoodGetKeyIndex(const KeyType& key, const ValueType& value) {
        ElementType to_insert(key, value);
        std::size_t current_psl = 1;
        ssize_t answer_i = -1;
        std::size_t i = hash_(key) % size_;

        while (!free_places_[i]) {
            if (psl_[i] < current_psl) {
                if (answer_i == -1) {
                    answer_i = i;
                }

                std::swap(data_[i], to_insert);
                std::swap(psl_[i], current_psl);
            }
            ++i;
            ++current_psl;
            if (i == size_) {
                i = 0;
            }
        }
        if (answer_i == -1) {
            answer_i = i;
        }
        InsertByIndex(i, to_insert.first, to_insert.second, current_psl);
        return static_cast<std::size_t>(answer_i);
    }
    std::size_t InsertElement(const KeyType& key, const ValueType& value) {
        if (!IsCoolLoad(size_, MAX_LOAD_FACTOR)) {
            Rebuild();
        }

        return RobinHoodGetKeyIndex(key, value);
    }

    void SwapInElementPositions(size_t i, size_t j) {
        std::swap(index_in_element_positions_[element_positions_[i]], index_in_element_positions_[element_positions_[j]]);
        std::swap(element_positions_[i], element_positions_[j]);
    }
    void FixPosition(size_t position) {
        std::size_t i = position + 1;
        if (i == size_) {
            i = 0;
        }
        std::queue<ElementType> elements;
        while (!free_places_[i]) {
            elements.push(std::move(data_[i]));
            DeleteElementByPosition(i, false);
            ++i;
            if (i == size_) {
                i = 0;
            }
        }
        while (!elements.empty()) {
            auto e = elements.front();
            elements.pop();
            InsertElement(e.first, e.second);
        }
    }
    void DeleteElementByPosition(size_t position, bool need_fix = true) {
        std::size_t i = index_in_element_positions_[position];
        free_places_[position] = true;
        size_t back = element_positions_.size();
        --back;
        SwapInElementPositions(i, back);
        element_positions_.pop_back();
        --element_count_;

        if (need_fix) {
            FixPosition(position);
        }
    }
    void DeleteElementByIndex(size_t i) {
        DeleteElementByPosition(element_positions_[i]);
    }

    Hash hash_;

    std::size_t size_ = 0;
    std::size_t element_count_ = 0;

    std::vector<ElementType> data_;
    std::vector<std::size_t> psl_;
    std::vector<bool> free_places_;
    std::vector<std::size_t> index_in_element_positions_;
    std::vector<std::size_t> element_positions_;
};
