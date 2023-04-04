// This file is derived from SomeProject
// Original Author: BusyStudent(https://github.com/BusyStudent/Btk-ng)
// Original License: MIT License

// This file is part of MyProject
// Author: llhsdmd(llhsdmd@qq.com)
// Copyright (c) 2021 Your Company

/*
MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so...
*/

#pragma once

#include <functional>
#include <list>
#include <memory>
#include <tuple>
#include <cstring>

#include "call.hpp"

// Internal func for queued connection
namespace _SlotPriv {

void *_GetQueue();
void _PushQueuedCall(void *, void (*fn)(void *), void *param);

} // namespace _SlotPriv

// Emm, Maybe we should use our clang-format to format it
// Formated with WebKit style
struct _QueuedConnection {};

// Special connection tags
inline constexpr auto QueuedConnection = _QueuedConnection{};

template <size_t Index, class T, class... Args> struct _IndexArgsPackImpl {
  using type = typename _IndexArgsPackImpl<Index - 1, Args...>::type;
};
template <class T, class... Args> struct _IndexArgsPackImpl<0, T, Args...> {
  using type = T;
};
/**
 * @brief Get Elem from type args package
 *
 * @tparam Index
 * @tparam Args
 */
template <size_t Index, class... Args> struct IndexArgsPack {
  static_assert(Index < sizeof...(Args), "Out of range");
  using type = typename _IndexArgsPackImpl<Index, Args...>::type;
};

template <class T> struct FunctionTraits {};
/**
 * @brief Traits for Function
 *
 * @tparam RetT
 * @tparam Args
 */
template <class RetT, class... Args> struct FunctionTraits<RetT (*)(Args...)> {
  using result_type = RetT;
  static constexpr size_t args_count = sizeof...(Args);

  template <size_t Index>
  using arg_type = typename IndexArgsPack<Index, Args...>::type;
};
/**
 * @brief Get info of a function
 *
 * @tparam RetT
 * @tparam Args
 */
template <class RetT, class... Args> struct FunctionTraits<RetT(Args...)> {
  using result_type = RetT;
  static constexpr size_t args_count = sizeof...(Args);

  template <size_t Index>
  using arg_type = typename IndexArgsPack<Index, Args...>::type;
};

template <class T> struct MemberFunctionTraits {};
/**
 * @brief Member function traits
 *
 * @tparam RetT
 * @tparam Class
 * @tparam Args
 */
template <class RetT, class Class, class... Args>
struct MemberFunctionTraits<RetT (Class::*)(Args...)> {
  using result_type = RetT;
  using object_type = Class;
  using class_type = Class;
  static constexpr size_t args_count = sizeof...(Args);
  static constexpr bool is_const = false;

  template <size_t Index>
  using arg_type = typename IndexArgsPack<Index, Args...>::type;
};
// for const method
template <class RetT, class Class, class... Args>
struct MemberFunctionTraits<RetT (Class::*)(Args...) const> {
  using result_type = RetT;
  using object_type = const Class;
  using class_type = Class;
  static constexpr size_t args_count = sizeof...(Args);
  static constexpr bool is_const = true;

  template <size_t Index>
  using arg_type = typename IndexArgsPack<Index, Args...>::type;
};
// C++17 void_t
template <class... Args> using void_t = void;

class Trackable;
struct _BindWithMemFunction {
  Trackable *object_ptr;
};
/**
 * @brief Binder for Wrap method with object to
 *
 * @tparam Method
 */
template <class Method> class _MemberFunctionBinder {
public:
  using object_type = typename MemberFunctionTraits<Method>::object_type;
  using result_type = typename MemberFunctionTraits<Method>::result_type;
  // Init
  _MemberFunctionBinder(Method m, object_type *c) : method(m), obj(c) {}
  // Class
  template <class... Args> result_type operator()(Args &&...args) const {
    return _Call(method, obj, std::forward<Args>(args)...);
  }

private:
  Method method;
  object_type *obj;
};
template <class T> struct _MemFunctionBinder : _BindWithMemFunction {
  _MemFunctionBinder(Trackable *o, T &&b) : binder(b) { object_ptr = o; }
  T binder;

  template <class... Args> auto operator()(Args &&...args) {
    return binder(std::forward<Args>(args)...);
  }
};

template <class... Args> inline auto Bind(Args &&...args) {
  return std::bind(std::forward<Args>(args)...);
}
template <class RetT, class Class, class... FnArgs, class... Args>
auto Bind(RetT (Class::*method)(FnArgs...), Class *object, Args &&...args) {
  return _MemFunctionBinder{
      object, std::bind(method, object, std::forward<Args>(args)...)};
}
template <class RetT, class Class, class... FnArgs, class... Args>
auto Bind(RetT (Class::*method)(FnArgs...) const, const Class *object,
          Args &&...args) {
  return _MemFunctionBinder{
      object, std::bind(method, object, std::forward<Args>(args)...)};
}

class SignalBase;
template <class RetT> class Signal;
class Trackable;
class _SlotBase;
struct _Functor;

/**
 * @brief FunctionLocation
 *
 */
struct _FunctorLocation {
  std::list<_Functor>::iterator iter;

  _Functor *operator->() { return iter.operator->(); }
};

/**
 * @brief Signal's connection
 *
 */
class Connection {
public:
  Connection() {}
  Connection(const Connection &con) { memcpy(this, &con, sizeof(Connection)); }
  ~Connection() {}

  SignalBase *signal() const noexcept { return sig.current; }
  void disconnect(bool from_object = false);

  Connection &operator=(const Connection &con) {
    memcpy(this, &con, sizeof(Connection));
    return *this;
  }

protected:
  typedef std::list<_SlotBase *>::iterator Iterator;
  union {
    struct {
      SignalBase *current; //< current signal
      Iterator iter;       //<The iterator of the slot ptr
    } sig;
    struct {
      Trackable *object;
      _FunctorLocation loc;
    } obj;
  };
  enum { WithSignal, WithObject, None } status;
  Connection(SignalBase *c, Iterator i) {
    sig.iter = i;
    sig.current = c;

    status = WithSignal;
  }
  Connection(Trackable *object, _FunctorLocation loc) {
    obj.object = object;
    obj.loc = loc;

    status = WithObject;
  }
  template <class RetT> friend class Signal;
  friend class Trackable;
};

/**
 * @brief Functor in Trackable for cleaningup data
 *
 */
struct _Functor {
  enum {
    Unknown, //< Unkonw callback
    Timer,   //< Callback for removeing timer
    Signal   //< Callback for removeing signal
  } magic = Unknown;
  // Userdata
  void *user1;
  void *user2;
  /**
   * @brief Call the _Functor,cleanup will not be called after it
   *
   */
  void (*call)(_Functor &) = nullptr; //<It could be nullptr
  /**
   * @brief Cleanup The _Functor
   *
   */
  void (*cleanup)(_Functor &) = nullptr; //<It could be nullptr

  bool reserved = false; //< Flag for call from Trackable
  bool reserved1 = false;
  bool reserved2 = false;
  bool reserved3 = false;

  // methods
  void _call() {
    if (call != nullptr) {
      call(*this);
    }
  }
  void _cleanup() {
    if (cleanup != nullptr) {
      cleanup(*this);
    }
  }
};
struct _TimerID {
  int id;
  /**
   * @brief Stop the timer
   *
   */
  void stop();
};
/**
 * @brief Basic slot
 *
 */
class _SlotBase {
protected:
  //< pointer for delete the slot
  typedef void (*CleanupFn)(void *self, bool from_object);
  CleanupFn cleanup_ptr;

  _SlotBase(CleanupFn f) : cleanup_ptr(f){};

private:
  /**
   * @brief Delete the slot
   *
   * @param from_object Is the Trackable::~Trackable call the method?
   */
  void cleanup(bool from_object = false) { cleanup_ptr(this, from_object); }
  template <class RetT> friend class Signal;
  friend class SignalBase;
  friend class Connection;
};
/**
 * @brief Slot interface for notify
 *
 * @tparam RetT
 * @tparam Args
 */
template <class RetT, class... Args> class _Slot : public _SlotBase {
protected:
  typedef RetT (*InvokeFn)(void *self, Args... args);
  InvokeFn invoke_ptr;
  /**
   * @brief Call the slot
   *
   * @param args
   * @return RetT
   */
  RetT invoke(Args... args) {
    return invoke_ptr(this, std::forward<Args>(args)...);
  }
  _Slot(CleanupFn c, InvokeFn i) : _SlotBase(c) { invoke_ptr = i; }
  template <class T> friend class Signal;
  friend class Connection;
};
/**
 * @brief Slot for callable object
 *
 * @tparam RetT
 * @tparam Args
 */
template <class Callable, class RetT, class... Args>
class _MemSlot : public _Slot<RetT, Args...> {
protected:
  Callable callable;
  /**
   * @brief Delete self
   *
   */
  static void Delete(void *self, bool) { delete static_cast<_MemSlot *>(self); }
  static RetT Invoke(void *self, Args... args) {
    return static_cast<_MemSlot *>(self)->invoke(std::forward<Args>(args)...);
  }
  RetT invoke(Args... args) { return callable(std::forward<Args>(args)...); }

  _MemSlot(Callable &&callable)
      : _Slot<RetT, Args...>(Delete, Invoke),
        callable(std::forward<Callable>(callable)) {}
  template <class T> friend class Signal;
  friend class Trackable;
};
/**
 * @brief Slot for Btk::HasSlots's member function
 *
 * @tparam Callable
 * @tparam RetT
 * @tparam Args
 */
template <class Callable, class RetT, class... Args>
class _ClassSlot : public _Slot<RetT, Args...> {
protected:
  Callable callable;
  Trackable *object;
  _FunctorLocation location;
  static void Delete(void *self, bool from_object) {
    std::unique_ptr<_ClassSlot> ptr(static_cast<_ClassSlot *>(self));
    // Call from object
    if (!from_object) {
      //< lock the object
      // Btk::lock_guard<Trackable> locker(*(ptr->object));

      ptr->object->Trackable::remove_callback(ptr->location);
    }
  }
  static RetT Invoke(void *self, Args... args) {
    return static_cast<_ClassSlot *>(self)->invoke(std::forward<Args>(args)...);
  }
  RetT invoke(Args... args) {
    return _Call(callable, std::forward<Args>(args)...);
  }
  _ClassSlot(Callable &&c, Trackable *o)
      : _Slot<RetT, Args...>(Delete, Invoke), callable(c), object(o) {}
  template <class T> friend class Signal;
  friend class Trackable;
};
class _GenericCallBase {
protected:
  bool deleted = false;
  friend struct _GenericCallFunctor;
};
/**
 * @brief A class for DeferCall or async
 *
 * @tparam Class
 * @tparam Method
 * @tparam Args
 */
template <class Class, class Method, class... Args>
class _GenericCallInvoker : public std::tuple<Class *, Args...>,
                            _GenericCallBase {
  Method method;
  /**
   * @brief Construct a new defercallinvoker object
   *
   * @param object
   * @param method
   */
  _GenericCallInvoker(Class *object, Method method, Args... args)
      : std::tuple<Class *, Args...>(object, std::forward<Args>(args)...) {
    this->method = method;
  }
  void invoke() {
    if (deleted) {
      return;
    }
    if (!object()->try_lock()) {
      //< Failed to lock,The object is cleanuping
      return;
    }
    // Btk::lock_guard<Trackable> locker(*(object()),Btk::adopt_lock);
    //< Call self
    std::apply(method, static_cast<std::tuple<Class *, Args...> &&>(*this));
  }
  Class *object() {
    return std::get<0>(static_cast<std::tuple<Class *, Args...> &&>(*this));
  }
  /*
   * @brief Main entry for DeferCall
   *
   * @param self
   */
  static void Run(void *self) {
    std::unique_ptr<_GenericCallInvoker> ptr(
        static_cast<_GenericCallInvoker *>(self));
    ptr->invoke();
  }
  friend class Trackable;
};
/**
 * @brief Helper class for defercall or async invoker
 *
 */
struct _GenericCallFunctor : public _Functor {
  _GenericCallFunctor(_GenericCallBase *defercall);
};
/**
 * @brief Generic object provide signals/_slots timer etc...
 *
 */
class Trackable {
public:
  Trackable();
  Trackable(const Trackable &) {}
  Trackable(Trackable &&) {}
  ~Trackable();
  using TimerID = _TimerID;
  using Functor = _Functor;
  using FunctorLocation = _FunctorLocation;

  // template<class Callable,class ...Args>
  // void on_destroy(Callable &&callable,Args &&...args){
  //     using Invoker = _OnceInvoker<Callable,Args...>;
  //     add_callback(
  //         Invoker::Run,
  //         new Invoker{
  //             {std::forward<Args>(args)...},
  //             std::forward<Callable>(callable)
  //         }
  //     );
  // }

  /**
   * @brief Disconnect all signal
   *
   */
  void disconnect_all() { return impl().disconnect_all(); }
  /**
   * @brief Disconnect all signal and remove all timer
   *
   */
  void cleanup() { return impl().cleanup(); }

  FunctorLocation add_callback(void (*fn)(void *), void *param) {
    return impl().add_callback(fn, param);
  }
  FunctorLocation add_functor(const Functor &f) {
    return impl().add_functor(f);
  }
  // Exec and remove
  FunctorLocation exec_functor(FunctorLocation location) {
    return impl().exec_functor(location);
  }
  // Remove
  FunctorLocation remove_callback(FunctorLocation location) {
    return impl().remove_callback(location);
  }
  /**
   * @brief Remove callback(safe to pass invalid location,but slower)
   *
   * @param location
   * @return FunctorLocation
   */
  FunctorLocation remove_callback_safe(FunctorLocation location) {
    return impl().remove_callback_safe(location);
  }

private:
  struct Impl {
    std::list<Functor> functors_cb;
    // mutable SpinLock spinlock;//<SDL_spinlock for multithreading

    // Member
    void disconnect_all();
    void cleanup();

    FunctorLocation add_callback(void (*fn)(void *), void *param);
    FunctorLocation add_functor(const Functor &);
    FunctorLocation exec_functor(FunctorLocation location);
    FunctorLocation remove_callback(FunctorLocation location);
    FunctorLocation remove_callback_safe(FunctorLocation location);
    // TimerID add_timer(Uint32 internal);

    // void dump_functors(FILE *output = stderr) const;
  };
  /**
   * @brief Get data,if _data == nullptr,it will create it
   *
   * @return Data&
   */
  Impl &impl() const;
  mutable Impl *_impl = nullptr; //<For lazy
  template <class RetT> friend class Signal;
};
/**
 * @brief Functor for Connection
 *
 */
struct _ConnectionFunctor : public _Functor {
  /**
   * @brief Construct a new connectfunctor object
   *
   * @param con The connection
   */
  _ConnectionFunctor(Connection con);
};
/**
 * @brief Basic signal
 *
 */
class SignalBase : public Trackable {
public:
  SignalBase();
  SignalBase(const SignalBase &) = delete;
  ~SignalBase();
  /**
   * @brief The signal is empty?
   *
   * @return true
   * @return false
   */
  bool empty() const { return _slots.empty(); }
  /**
   * @brief The signal is emitting?
   *
   * @return true
   * @return false
   */
  // bool emitting() const{
  //     return spinlock.is_lock();
  // }
  bool operator==(std::nullptr_t) const { return empty(); }
  bool operator!=(std::nullptr_t) const { return !empty(); }
  operator bool() const { return !empty(); }
  /**
   * @brief Debug for dump
   *
   * @param output
   */
  // void dump_slots(FILE *output = stderr) const;
  void disconnect_all();

protected:
  std::list<_SlotBase *> _slots; //< All _slots
  // mutable SpinLock spinlock;//< SDL_spinlock for multithreading
  template <class RetT> friend class Signal;
  friend class Connection;
};
/**
 * @brief Signals
 *
 * @tparam RetT Return types
 * @tparam Args The args
 */
template <class RetT, class... Args>
class Signal<RetT(Args...)> : public SignalBase {
public:
  using SignalBase::SignalBase;
  using result_type = RetT;
  /**
   * @brief Emit the signal
   *
   * @param args The args
   * @return RetT The return type
   */
  RetT notify(Args... args) const {
    // lock the signalbase
    //  lock_guard<const SignalBase> locker(*this);
    // why it has complie error on msvc
    // std::is_same<void,RetT>()
    if (empty()) {
      return RetT();
    }

    if constexpr (std::is_same<void, RetT>::value) {
#if 1
      // FIXME : It may be slow, but it as safe when user disconnect
      // the signal in the callback
      auto _slots = this->_slots;
#endif

      for (auto slot : _slots) {
        static_cast<_Slot<RetT, Args...> *>(slot)->invoke(
            std::forward<Args>(args)...);
      }
    } else {
#if 1
      // FIXME : It may be slow, but it as safe when user disconnect
      // the signal in the callback
      auto _slots = this->_slots;
#endif
      RetT ret{};
      for (auto slot : _slots) {
        ret = static_cast<_Slot<RetT, Args...> *>(slot)->invoke(
            std::forward<Args>(args)...);
      }
      return ret;
    }
  }
  /**
   * @brief Emit with nothrow
   *
   * @param args
   * @return RetT
   */
  RetT nothrow_emit(Args... args) const {
    // lock the signalbase
    //  lock_guard<const SignalBase> locker(*this);
    // why it has complie error on msvc
    // std::is_same<void,RetT>()
    if constexpr (std::is_same<void, RetT>::value) {
      for (auto slot : _slots) {
        static_cast<_Slot<RetT, Args...> *>(slot)->invoke(
            std::forward<Args>(args)...);
      }
    } else {
      RetT ret{};
      for (auto slot : _slots) {
        ret = static_cast<_Slot<RetT, Args...> *>(slot)->invoke(
            std::forward<Args>(args)...);
      }
      return ret;
    }
  }
  /**
   * @brief Push the notify into event queue
   *
   * @param args
   */
  void defer_emit(Args... args) {
    if (empty()) {
      return;
    }
    defer_call(&Signal::defer_emit_entry, std::forward<Args>(args)...);
  }
  RetT operator()(Args... args) const {
    return notify(std::forward<Args>(args)...);
  }
  /**
   * @brief Connect callable
   *
   * @tparam Callable
   * @param callable
   * @return Connection
   */
  template <class Callable> Connection connect(Callable &&callable) {
    // lock_guard<const SignalBase> locker(*this);

    if constexpr (std::is_base_of_v<_BindWithMemFunction, Callable>) {
      // For callable bind with HasSlots
      using Slot = _ClassSlot<Callable, RetT, Args...>;

      Trackable *object =
          static_cast<_BindWithMemFunction &>(callable).object_ptr;

      Slot *slot = new Slot(std::forward<Callable>(callable), object);
      _slots.push_back(slot);
      // make connection
      Connection con = {this, --_slots.end()};

      _ConnectionFunctor functor(con);

      slot->location = object->Trackable::add_functor(functor);
      return {object, slot->location};
    } else {
      // For common callable
      using Slot = _MemSlot<Callable, RetT, Args...>;
      _slots.push_back(new Slot(std::forward<Callable>(callable)));
      return Connection{this, --_slots.end()};
    }
  }
  template <class Method, class TObject>
  Connection connect(Method &&method, TObject *object) {
    static_assert(std::is_base_of<Trackable, TObject>(),
                  "Trackable must inherit HasSlots");
    // lock_guard<const SignalBase> locker(*this);

    using ClassWrap = _MemberFunctionBinder<Method>;
    using Slot = _ClassSlot<ClassWrap, RetT, Args...>;
    Slot *slot = new Slot(ClassWrap(method, object), object);
    _slots.push_back(slot);
    // make connection
    Connection con = {this, --_slots.end()};

    _ConnectionFunctor functor(con);

    slot->location = object->Trackable::add_functor(functor);

    return {object, slot->location};
  }

private:
  //< Impl for defer notify
  void defer_emit_entry(Args... args) { notify(std::forward<Args>(args)...); }
};
using HasSlots = Trackable;
