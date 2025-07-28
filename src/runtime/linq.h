#ifndef TOCIN_LINQ_H
#define TOCIN_LINQ_H

#include <vector>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <optional>

namespace runtime {

// Forward declarations
template<typename T>
class Queryable;
class QueryExpression;
class QueryBuilder;

/**
 * @brief LINQ query operators
 */
enum class QueryOperator {
    SELECT,
    WHERE,
    ORDER_BY,
    ORDER_BY_DESCENDING,
    GROUP_BY,
    JOIN,
    LEFT_JOIN,
    RIGHT_JOIN,
    FULL_JOIN,
    DISTINCT,
    TAKE,
    SKIP,
    FIRST,
    FIRST_OR_DEFAULT,
    LAST,
    LAST_OR_DEFAULT,
    SINGLE,
    SINGLE_OR_DEFAULT,
    ANY,
    ALL,
    CONTAINS,
    COUNT,
    SUM,
    AVERAGE,
    MIN,
    MAX,
    AGGREGATE,
    UNION,
    INTERSECT,
    EXCEPT,
    CONCAT,
    REVERSE,
    TO_LIST,
    TO_ARRAY,
    TO_DICTIONARY,
    TO_LOOKUP
};

/**
 * @brief Query expression node
 */
class QueryNode {
public:
    virtual ~QueryNode() = default;
    virtual QueryOperator getOperator() const = 0;
    virtual std::string toString() const = 0;
};

/**
 * @brief Select query node
 */
class SelectNode : public QueryNode {
private:
    std::string selector;

public:
    explicit SelectNode(const std::string& sel) : selector(sel) {}

    QueryOperator getOperator() const override { return QueryOperator::SELECT; }
    std::string toString() const override { return "Select(" + selector + ")"; }
    const std::string& getSelector() const { return selector; }
};

/**
 * @brief Where query node
 */
class WhereNode : public QueryNode {
private:
    std::string predicate;

public:
    explicit WhereNode(const std::string& pred) : predicate(pred) {}

    QueryOperator getOperator() const override { return QueryOperator::WHERE; }
    std::string toString() const override { return "Where(" + predicate + ")"; }
    const std::string& getPredicate() const { return predicate; }
};

/**
 * @brief OrderBy query node
 */
class OrderByNode : public QueryNode {
private:
    std::string key_selector;
    bool descending;

public:
    OrderByNode(const std::string& key, bool desc = false) 
        : key_selector(key), descending(desc) {}

    QueryOperator getOperator() const override { 
        return descending ? QueryOperator::ORDER_BY_DESCENDING : QueryOperator::ORDER_BY; 
    }
    std::string toString() const override { 
        return (descending ? "OrderByDescending(" : "OrderBy(") + key_selector + ")"; 
    }
    const std::string& getKeySelector() const { return key_selector; }
    bool isDescending() const { return descending; }
};

/**
 * @brief GroupBy query node
 */
class GroupByNode : public QueryNode {
private:
    std::string key_selector;
    std::string element_selector;

public:
    GroupByNode(const std::string& key, const std::string& element = "")
        : key_selector(key), element_selector(element) {}

    QueryOperator getOperator() const override { return QueryOperator::GROUP_BY; }
    std::string toString() const override { 
        return "GroupBy(" + key_selector + (element_selector.empty() ? "" : ", " + element_selector) + ")"; 
    }
    const std::string& getKeySelector() const { return key_selector; }
    const std::string& getElementSelector() const { return element_selector; }
};

/**
 * @brief Join query node
 */
class JoinNode : public QueryNode {
private:
    std::string inner_sequence;
    std::string outer_key_selector;
    std::string inner_key_selector;
    std::string result_selector;
    QueryOperator join_type;

public:
    JoinNode(const std::string& inner, const std::string& outer_key, 
             const std::string& inner_key, const std::string& result, 
             QueryOperator type = QueryOperator::JOIN)
        : inner_sequence(inner), outer_key_selector(outer_key), 
          inner_key_selector(inner_key), result_selector(result), join_type(type) {}

    QueryOperator getOperator() const override { return join_type; }
    std::string toString() const override { 
        std::string type_str = (join_type == QueryOperator::LEFT_JOIN) ? "LeftJoin" :
                              (join_type == QueryOperator::RIGHT_JOIN) ? "RightJoin" :
                              (join_type == QueryOperator::FULL_JOIN) ? "FullJoin" : "Join";
        return type_str + "(" + inner_sequence + ", " + outer_key_selector + ", " + 
               inner_key_selector + ", " + result_selector + ")"; 
    }
    const std::string& getInnerSequence() const { return inner_sequence; }
    const std::string& getOuterKeySelector() const { return outer_key_selector; }
    const std::string& getInnerKeySelector() const { return inner_key_selector; }
    const std::string& getResultSelector() const { return result_selector; }
};

/**
 * @brief Take/Skip query node
 */
class TakeSkipNode : public QueryNode {
private:
    int count;
    bool is_take;

public:
    TakeSkipNode(int c, bool take) : count(c), is_take(take) {}

    QueryOperator getOperator() const override { 
        return is_take ? QueryOperator::TAKE : QueryOperator::SKIP; 
    }
    std::string toString() const override { 
        return (is_take ? "Take(" : "Skip(") + std::to_string(count) + ")"; 
    }
    int getCount() const { return count; }
    bool isTake() const { return is_take; }
};

/**
 * @brief Queryable collection with LINQ support
 */
template<typename T>
class Queryable {
private:
    std::vector<T> data;
    std::vector<std::shared_ptr<QueryNode>> query_nodes;

public:
    Queryable() = default;
    
    explicit Queryable(const std::vector<T>& items) : data(items) {}
    
    explicit Queryable(std::vector<T>&& items) : data(std::move(items)) {}

    /**
     * @brief Add a query node
     */
    void addQueryNode(std::shared_ptr<QueryNode> node) {
        query_nodes.push_back(node);
    }

    /**
     * @brief Get the underlying data
     */
    const std::vector<T>& getData() const { return data; }

    /**
     * @brief Get query nodes
     */
    const std::vector<std::shared_ptr<QueryNode>>& getQueryNodes() const { return query_nodes; }

    /**
     * @brief Execute the query and return results
     */
    std::vector<T> execute() const {
        std::vector<T> result = data;
        
        for (const auto& node : query_nodes) {
            switch (node->getOperator()) {
                case QueryOperator::WHERE: {
                    auto where_node = std::dynamic_pointer_cast<WhereNode>(node);
                    if (where_node) {
                        result = applyWhere(result, where_node->getPredicate());
                    }
                    break;
                }
                case QueryOperator::SELECT: {
                    auto select_node = std::dynamic_pointer_cast<SelectNode>(node);
                    if (select_node) {
                        result = applySelect(result, select_node->getSelector());
                    }
                    break;
                }
                case QueryOperator::ORDER_BY:
                case QueryOperator::ORDER_BY_DESCENDING: {
                    auto order_node = std::dynamic_pointer_cast<OrderByNode>(node);
                    if (order_node) {
                        result = applyOrderBy(result, order_node->getKeySelector(), order_node->isDescending());
                    }
                    break;
                }
                case QueryOperator::TAKE: {
                    auto take_node = std::dynamic_pointer_cast<TakeSkipNode>(node);
                    if (take_node && take_node->isTake()) {
                        result = applyTake(result, take_node->getCount());
                    }
                    break;
                }
                case QueryOperator::SKIP: {
                    auto skip_node = std::dynamic_pointer_cast<TakeSkipNode>(node);
                    if (skip_node && !skip_node->isTake()) {
                        result = applySkip(result, skip_node->getCount());
                    }
                    break;
                }
                case QueryOperator::DISTINCT: {
                    result = applyDistinct(result);
                    break;
                }
                case QueryOperator::REVERSE: {
                    result = applyReverse(result);
                    break;
                }
                default:
                    // Other operators would be implemented here
                    break;
            }
        }
        
        return result;
    }

    /**
     * @brief LINQ methods
     */
    Queryable<T> where(const std::string& predicate) const {
        Queryable<T> result = *this;
        result.addQueryNode(std::make_shared<WhereNode>(predicate));
        return result;
    }

    template<typename U>
    Queryable<U> select(const std::string& selector) const {
        Queryable<U> result;
        result.addQueryNode(std::make_shared<SelectNode>(selector));
        return result;
    }

    Queryable<T> orderBy(const std::string& key_selector) const {
        Queryable<T> result = *this;
        result.addQueryNode(std::make_shared<OrderByNode>(key_selector, false));
        return result;
    }

    Queryable<T> orderByDescending(const std::string& key_selector) const {
        Queryable<T> result = *this;
        result.addQueryNode(std::make_shared<OrderByNode>(key_selector, true));
        return result;
    }

    Queryable<T> take(int count) const {
        Queryable<T> result = *this;
        result.addQueryNode(std::make_shared<TakeSkipNode>(count, true));
        return result;
    }

    Queryable<T> skip(int count) const {
        Queryable<T> result = *this;
        result.addQueryNode(std::make_shared<TakeSkipNode>(count, false));
        return result;
    }

    Queryable<T> distinct() const {
        Queryable<T> result = *this;
        result.addQueryNode(std::make_shared<QueryNode>()); // Placeholder for DISTINCT
        return result;
    }

    Queryable<T> reverse() const {
        Queryable<T> result = *this;
        result.addQueryNode(std::make_shared<QueryNode>()); // Placeholder for REVERSE
        return result;
    }

    // Aggregation methods
    T first() const {
        auto result = execute();
        return result.empty() ? T{} : result.front();
    }

    T firstOrDefault() const {
        auto result = execute();
        return result.empty() ? T{} : result.front();
    }

    T last() const {
        auto result = execute();
        return result.empty() ? T{} : result.back();
    }

    T lastOrDefault() const {
        auto result = execute();
        return result.empty() ? T{} : result.back();
    }

    T single() const {
        auto result = execute();
        if (result.size() != 1) {
            throw std::runtime_error("Sequence contains more than one element");
        }
        return result.front();
    }

    T singleOrDefault() const {
        auto result = execute();
        if (result.size() > 1) {
            throw std::runtime_error("Sequence contains more than one element");
        }
        return result.empty() ? T{} : result.front();
    }

    bool any() const {
        return !execute().empty();
    }

    bool any(const std::string& predicate) const {
        auto result = where(predicate).execute();
        return !result.empty();
    }

    bool all(const std::string& predicate) const {
        auto result = where(predicate).execute();
        return result.size() == execute().size();
    }

    bool contains(const T& item) const {
        auto result = execute();
        return std::find(result.begin(), result.end(), item) != result.end();
    }

    int count() const {
        return static_cast<int>(execute().size());
    }

    int count(const std::string& predicate) const {
        return where(predicate).count();
    }

    // Numeric aggregation methods (for numeric types)
    template<typename U = T>
    typename std::enable_if<std::is_arithmetic<U>::value, U>::type sum() const {
        auto result = execute();
        return std::accumulate(result.begin(), result.end(), U{});
    }

    template<typename U = T>
    typename std::enable_if<std::is_arithmetic<U>::value, double>::type average() const {
        auto result = execute();
        if (result.empty()) return 0.0;
        return static_cast<double>(std::accumulate(result.begin(), result.end(), U{})) / result.size();
    }

    template<typename U = T>
    typename std::enable_if<std::is_arithmetic<U>::value, U>::type min() const {
        auto result = execute();
        if (result.empty()) return U{};
        return *std::min_element(result.begin(), result.end());
    }

    template<typename U = T>
    typename std::enable_if<std::is_arithmetic<U>::value, U>::type max() const {
        auto result = execute();
        if (result.empty()) return U{};
        return *std::max_element(result.begin(), result.end());
    }

private:
    // Helper methods for query execution
    std::vector<T> applyWhere(const std::vector<T>& items, const std::string& predicate) const {
        // This would be implemented with a predicate evaluator
        // For now, return the original items
        return items;
    }

    template<typename U>
    std::vector<U> applySelect(const std::vector<T>& items, const std::string& selector) const {
        // This would be implemented with a selector evaluator
        // For now, return empty vector
        return std::vector<U>{};
    }

    std::vector<T> applyOrderBy(const std::vector<T>& items, const std::string& key_selector, bool descending) const {
        // This would be implemented with a key selector evaluator
        // For now, return the original items
        return items;
    }

    std::vector<T> applyTake(const std::vector<T>& items, int count) const {
        std::vector<T> result;
        result.reserve(std::min(static_cast<size_t>(count), items.size()));
        std::copy_n(items.begin(), std::min(static_cast<size_t>(count), items.size()), std::back_inserter(result));
        return result;
    }

    std::vector<T> applySkip(const std::vector<T>& items, int count) const {
        if (count >= static_cast<int>(items.size())) {
            return std::vector<T>{};
        }
        return std::vector<T>(items.begin() + count, items.end());
    }

    std::vector<T> applyDistinct(const std::vector<T>& items) const {
        std::vector<T> result;
        std::unordered_set<T> seen;
        for (const auto& item : items) {
            if (seen.insert(item).second) {
                result.push_back(item);
            }
        }
        return result;
    }

    std::vector<T> applyReverse(const std::vector<T>& items) const {
        std::vector<T> result = items;
        std::reverse(result.begin(), result.end());
        return result;
    }
};

/**
 * @brief Query builder for fluent LINQ syntax
 */
template<typename T>
class QueryBuilder {
private:
    Queryable<T> queryable;

public:
    explicit QueryBuilder(const std::vector<T>& data) : queryable(data) {}
    explicit QueryBuilder(std::vector<T>&& data) : queryable(std::move(data)) {}

    /**
     * @brief Build the query
     */
    Queryable<T> build() const {
        return queryable;
    }

    /**
     * @brief LINQ method chaining
     */
    QueryBuilder<T> where(const std::string& predicate) {
        queryable = queryable.where(predicate);
        return *this;
    }

    template<typename U>
    QueryBuilder<U> select(const std::string& selector) {
        return QueryBuilder<U>(queryable.select<U>(selector));
    }

    QueryBuilder<T> orderBy(const std::string& key_selector) {
        queryable = queryable.orderBy(key_selector);
        return *this;
    }

    QueryBuilder<T> orderByDescending(const std::string& key_selector) {
        queryable = queryable.orderByDescending(key_selector);
        return *this;
    }

    QueryBuilder<T> take(int count) {
        queryable = queryable.take(count);
        return *this;
    }

    QueryBuilder<T> skip(int count) {
        queryable = queryable.skip(count);
        return *this;
    }

    QueryBuilder<T> distinct() {
        queryable = queryable.distinct();
        return *this;
    }

    QueryBuilder<T> reverse() {
        queryable = queryable.reverse();
        return *this;
    }

    /**
     * @brief Execute the query
     */
    std::vector<T> toList() const {
        return queryable.execute();
    }

    std::vector<T> toArray() const {
        return queryable.execute();
    }
};

/**
 * @brief LINQ extension methods
 */
namespace linq {

/**
 * @brief Create a queryable from a vector
 */
template<typename T>
Queryable<T> from(const std::vector<T>& data) {
    return Queryable<T>(data);
}

/**
 * @brief Create a queryable from a vector (rvalue)
 */
template<typename T>
Queryable<T> from(std::vector<T>&& data) {
    return Queryable<T>(std::move(data));
}

/**
 * @brief Create a query builder
 */
template<typename T>
QueryBuilder<T> query(const std::vector<T>& data) {
    return QueryBuilder<T>(data);
}

/**
 * @brief Create a query builder (rvalue)
 */
template<typename T>
QueryBuilder<T> query(std::vector<T>&& data) {
    return QueryBuilder<T>(std::move(data));
}

/**
 * @brief Range query
 */
Queryable<int> range(int start, int count) {
    std::vector<int> data;
    data.reserve(count);
    for (int i = 0; i < count; ++i) {
        data.push_back(start + i);
    }
    return Queryable<int>(std::move(data));
}

/**
 * @brief Repeat query
 */
template<typename T>
Queryable<T> repeat(const T& element, int count) {
    std::vector<T> data(count, element);
    return Queryable<T>(std::move(data));
}

/**
 * @brief Empty query
 */
template<typename T>
Queryable<T> empty() {
    return Queryable<T>();
}

} // namespace linq

} // namespace runtime

#endif // TOCIN_LINQ_H
