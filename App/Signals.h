#pragma once

#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>
#include <vector>

namespace Signals {
    class Connection {
    public:
        Connection() = default;
        Connection(const Connection&) = default;
        Connection& operator=(const Connection&) = default;
        Connection(Connection&&) noexcept = default;
        Connection& operator=(Connection&&) noexcept = default;

        void disconnect() {
            if (auto s = state.lock()) {
                s->connected = false;
            }
        }

        [[nodiscard]] bool connected() const noexcept {
            auto s = state.lock();
            return s && s->connected;
        }

        explicit operator bool() const noexcept { return connected(); }

    private:
        struct State {
            bool connected = true;
        };

        explicit Connection(std::shared_ptr<State> state) : state(std::move(state)) {}

        std::weak_ptr<State> state;

        template <typename> friend class Signal;
    };

    class ScopedConnection {
    public:
        ScopedConnection() = default;

        ScopedConnection(Connection conn) : conn(std::move(conn)) {}

        ScopedConnection(const ScopedConnection&) = delete;
        ScopedConnection& operator=(const ScopedConnection&) = delete;

        ScopedConnection(ScopedConnection&&) noexcept = default;
        ScopedConnection& operator=(ScopedConnection&& other) noexcept {
            if (this != &other) {
                conn.disconnect();
                conn = std::move(other.conn);
            }
            return *this;
        }

        ~ScopedConnection() { conn.disconnect(); }

        void disconnect() { conn.disconnect(); }
        [[nodiscard]] bool connected() const noexcept { return conn.connected(); }
        explicit operator bool() const noexcept { return connected(); }

        Connection release() {
            auto c = conn;
            conn = {};
            return c;
        }

    private:
        Connection conn;
    };

    template <typename Signature> class Signal;

    template <typename R, typename... Args> class Signal<R(Args...)> {
    public:
        using SlotFn = std::function<R(Args...)>;
        using result_type = R;

        Signal() = default;
        ~Signal() = default;

        Signal(const Signal&) = delete;
        Signal& operator=(const Signal&) = delete;
        Signal(Signal&&) noexcept = default;
        Signal& operator=(Signal&&) noexcept = default;

        template <typename Fn>
            requires std::invocable<Fn, Args...>
        [[nodiscard]] Connection connect(Fn&& fn) {
            auto state = std::make_shared<Connection::State>();
            slots.emplace_back(std::forward<Fn>(fn), state);
            return Connection{state};
        }

        template <typename T> [[nodiscard]] Connection connect(R (T::*method)(Args...), T* obj) {
            assert(obj != nullptr);
            return connect([obj, method](Args... args) -> R { return (obj->*method)(args...); });
        }

        template <typename T> [[nodiscard]] Connection connect(R (T::*method)(Args...) const, const T* obj) {
            assert(obj != nullptr);
            return connect([obj, method](Args... args) -> R { return (obj->*method)(args...); });
        }

        template <typename T> [[nodiscard]] Connection connect(R (T::*method)(Args...), std::shared_ptr<T> obj) {
            std::weak_ptr<T> weak = obj;
            return connect([weak, method](Args... args) -> R {
                if (auto locked = weak.lock()) {
                    return (locked.get()->*method)(args...);
                }
                if constexpr (!std::is_void_v<R>) {
                    return R{};
                }
            });
        }

        template <typename T> [[nodiscard]] Connection connect(R (T::*method)(Args...) const, std::shared_ptr<T> obj) {
            std::weak_ptr<T> weak = obj;
            return connect([weak, method](Args... args) -> R {
                if (auto locked = weak.lock()) {
                    return (locked.get()->*method)(args...);
                }
                if constexpr (!std::is_void_v<R>) {
                    return R{};
                }
            });
        }

        void emit(Args... args) {
            auto slots_copy = slots;
            bool has_dead = false;

            for (auto& [fn, state] : slots_copy) {
                if (!state->connected) {
                    has_dead = true;
                    continue;
                }
                fn(args...);
            }

            if (has_dead) {
                collect_garbage();
            }
        }

        void operator()(Args... args) { emit(args...); }

        void disconnect_all() noexcept {
            for (auto& [fn, state] : slots) {
                state->connected = false;
            }
            slots.clear();
        }

        [[nodiscard]] std::size_t slot_count() const noexcept {
            return std::count_if(slots.begin(), slots.end(), [](const Slot& s) { return s.state->connected; });
        }

        [[nodiscard]] bool empty() const noexcept { return slot_count() == 0; }

    private:
        struct Slot {
            SlotFn fn;
            std::shared_ptr<Connection::State> state;
        };

        std::vector<Slot> slots;

        void collect_garbage() {
            std::erase_if(slots, [](const Slot& s) { return !s.state->connected; });
        }
    };

    class Trackable {
    protected:
        void track(ScopedConnection conn) { connections_.emplace_back(std::move(conn)); }

    private:
        std::vector<ScopedConnection> connections_;
    };
}
