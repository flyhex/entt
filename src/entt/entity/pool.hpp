#ifndef ENTT_ENTITY_POOL_HPP
#define ENTT_ENTITY_POOL_HPP


#include <iterator>
#include <type_traits>
#include <utility>
#include "../config/config.h"
#include "../core/type_traits.hpp"
#include "../signal/sigh.hpp"
#include "fwd.hpp"
#include "storage.hpp"


namespace entt {


/**
 * @brief Default pool implementation.
 * @tparam Entity A valid entity type (see entt_traits for more details).
 * @tparam Type Type of objects assigned to the entities.
 */
template<typename Entity, typename Type>
struct default_pool final: storage<Entity, Type> {
    static_assert(std::is_same_v<Type, std::decay_t<Type>>, "Invalid object type");

    /*! @brief Type of the objects associated with the entities. */
    using object_type = Type;
    /*! @brief Underlying entity identifier. */
    using entity_type = Entity;

    /**
     * @brief Returns a sink object.
     *
     * The sink returned by this function can be used to receive notifications
     * whenever a new instance is created and assigned to an entity.<br/>
     * The function type for a listener is equivalent to:
     *
     * @code{.cpp}
     * void(basic_registry<Entity> &, Entity);
     * @endcode
     *
     * Listeners are invoked **after** the object has been assigned to the
     * entity.
     *
     * @sa sink
     *
     * @return A temporary sink object.
     */
    [[nodiscard]] auto on_construct() ENTT_NOEXCEPT {
        return sink{construction};
    }

    /**
     * @brief Returns a sink object for the given type.
     *
     * The sink returned by this function can be used to receive notifications
     * whenever an instance is explicitly updated.<br/>
     * The function type for a listener is equivalent to:
     *
     * @code{.cpp}
     * void(basic_registry<Entity> &, Entity);
     * @endcode
     *
     * Listeners are invoked **after** the object has been updated.
     *
     * @sa sink
     *
     * @return A temporary sink object.
     */
    [[nodiscard]] auto on_update() ENTT_NOEXCEPT {
        return sink{update};
    }

    /**
     * @brief Returns a sink object for the given type.
     *
     * The sink returned by this function can be used to receive notifications
     * whenever an instance is removed from an entity and thus destroyed.<br/>
     * The function type for a listener is equivalent to:
     *
     * @code{.cpp}
     * void(basic_registry<Entity> &, Entity);
     * @endcode
     *
     * Listeners are invoked **before** the object has been removed from the
     * entity.
     *
     * @sa sink
     *
     * @return A temporary sink object.
     */
    [[nodiscard]] auto on_destroy() ENTT_NOEXCEPT {
        return sink{destruction};
    }

    /**
     * @brief Assigns an entity to a pool.
     *
     * A new object is created and initialized with the arguments provided (the
     * object type must have a proper constructor or be of aggregate type). Then
     * the instance is assigned to the given entity.
     *
     * @warning
     * Attempting to use an invalid entity or to assign an entity that already
     * belongs to the pool results in undefined behavior.<br/>
     * An assertion will abort the execution at runtime in debug mode in case of
     * invalid entity or if the entity already belongs to the pool.
     *
     * @tparam Args Types of arguments to use to construct the object.
     * @param owner The registry that issued the request.
     * @param entity A valid entity identifier.
     * @param args Parameters to use to initialize the object.
     * @return A reference to the newly created object.
     */
    template<typename... Args>
    decltype(auto) emplace(basic_registry<entity_type> &owner, const entity_type entity, Args &&... args) {
        storage<entity_type, Type>::emplace(entity, std::forward<Args>(args)...);
        construction.publish(owner, entity);

        if constexpr(!is_eto_eligible_v<object_type>) {
            return this->get(entity);
        }
    }

    /**
     * @brief Assigns multiple entities to a pool.
     *
     * @sa emplace
     *
     * @tparam It Type of input iterator.
     * @tparam Args Types of arguments to use to construct the objects
     * associated with the entities.
     * @param owner The registry that issued the request.
     * @param first An iterator to the first element of the range of entities.
     * @param last An iterator past the last element of the range of entities.
     * @param args Parameters to use to initialize the objects associated with
     * the entities.
     */
    template<typename It, typename... Args>
    void insert(basic_registry<entity_type> &owner, It first, It last, Args &&... args) {
        storage<entity_type, object_type>::insert(first, last, std::forward<Args>(args)...);

        if(!construction.empty()) {
            for(; first != last; ++first) {
                construction.publish(owner, *first);
            }
        }
    }

    /**
     * @brief Removes an entity from a pool.
     *
     * @warning
     * Attempting to use an invalid entity or to remove an entity that doesn't
     * belong to the pool results in undefined behavior.<br/>
     * An assertion will abort the execution at runtime in debug mode in case of
     * invalid entity or if the entity doesn't belong to the pool.
     *
     * @param owner The registry that issued the request.
     * @param entity A valid entity identifier.
     */
    void erase(basic_registry<entity_type> &owner, const entity_type entity) {
        destruction.publish(owner, entity);
        storage<entity_type, object_type>::erase(entity);
    }

    /**
     * @brief Removes multiple entities from a pool.
     *
     * @see remove
     *
     * @tparam It Type of input iterator.
     * @param owner The registry that issued the request.
     * @param first An iterator to the first element of the range of entities.
     * @param last An iterator past the last element of the range of entities.
     */
    template<typename It>
    void erase(basic_registry<entity_type> &owner, It first, It last) {
        if(std::distance(first, last) == std::distance(this->begin(), this->end())) {
            if(!destruction.empty()) {
                for(; first != last; ++first) {
                    destruction.publish(owner, *first);
                }
            }

            this->clear();
        } else {
            for(; first != last; ++first) {
                this->erase(owner, *first);
            }
        }
    }

    /**
     * @brief Patches the given instance for an entity.
     *
     * The signature of the functions should be equivalent to the following:
     *
     * @code{.cpp}
     * void(Type &);
     * @endcode
     *
     * @note
     * Empty types aren't explicitly instantiated and therefore they are never
     * returned. However, this function can be used to trigger an update signal
     * for them.
     *
     * @warning
     * Attempting to use an invalid entity or to patch an object of an entity
     * that doesn't belong to the pool results in undefined behavior.<br/>
     * An assertion will abort the execution at runtime in debug mode in case of
     * invalid entity or if the entity doesn't belong to the pool.
     *
     * @tparam Func Types of the function objects to invoke.
     * @param owner The registry that issued the request.
     * @param entity A valid entity identifier.
     * @param func Valid function objects.
     * @return A reference to the patched instance.
     */
    template<typename... Func>
    decltype(auto) patch(basic_registry<entity_type> &owner, const entity_type entity, [[maybe_unused]] Func &&... func) {
        if constexpr(is_eto_eligible_v<object_type>) {
            update.publish(owner, entity);
        } else {
            (std::forward<Func>(func)(this->get(entity)), ...);
            update.publish(owner, entity);
            return this->get(entity);
        }
    }

private:
    sigh<void(basic_registry<entity_type> &, const entity_type)> construction{};
    sigh<void(basic_registry<entity_type> &, const entity_type)> destruction{};
    sigh<void(basic_registry<entity_type> &, const entity_type)> update{};
};


/**
 * @brief Applies component-to-pool and component-to-view conversions.
 * 
 * It defines the resulting pool type as the member typedef `type` and the
 * resulting view type as the member typedef `view`.
 * 
 * Formally:
 *
 * * The member typedef `type` is the default pool specialized with a non-const
 *   representation of the given type.
 * * If the component type is a non-const one, the member typedef `view` is the
 *   declared pool type.
 * * If the component type is a const one, the member typedef `view` is the
 *   declared pool type, except it has a const-qualifier added.
 * 
 * @tparam Entity A valid entity type (see entt_traits for more details).
 * @tparam Type Type of objects assigned to the entities.
 */
template<typename Entity, typename Type, typename = void>
struct pool {
    /*! @brief Resulting type after component-to-pool conversion. */
    using type = std::conditional_t<std::is_const_v<Type>, const default_pool<Entity, std::remove_const_t<Type>>, default_pool<Entity, Type>>;
    /*! @brief Resulting type after component-to-view conversion. */
    using view = type;
};


/**
 * @brief Alias declaration to use for component-to-pool conversions.
 * @tparam Entity A valid entity type (see entt_traits for more details).
 * @tparam Type Type of objects assigned to the entities.
 */
template<typename Entity, typename Type>
using pool_t = typename pool<Entity, Type>::type;


/**
 * @brief Alias declaration to use for component-to-view conversions.
 * @tparam Entity A valid entity type (see entt_traits for more details).
 * @tparam Type Type of objects assigned to the entities.
 */
template<typename Entity, typename Type>
using view_t = typename pool<Entity, Type>::view;


/**
 * @brief Helper variable template.
 * @tparam Type The type of which to extract the configuration method.
 */
template<typename Type>
inline constexpr auto view_config_v = pool<Entity, Type>::config;


}


#endif
