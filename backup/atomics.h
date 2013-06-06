template <class T> static T atomic_load_weak(volatile T *p) {
#ifdef __clang__
    T v;
    __atomic_load(p, &v, __ATOMIC_RELAXED);
    return v;
#else
    return *p;
#endif
}
template <class T> static T atomic_load_strong(volatile T *p) {
#ifdef __clang__
    T v;
    __atomic_load(p, &v, __ATOMIC_SEQ_CST);
    return v;
#else
    return *p; // gcc 4.7.2 doesn't like that atomic load.
#endif
}

template <class T> static void atomic_store_weak(volatile T *p, T v) {
#ifdef __clang__
    __atomic_store(p, &v, __ATOMIC_SEQ_CST);
#else
    *p = v;
#endif
}

template <class T> static void atomic_store_strong(volatile T *p, T v) {
#ifdef __clang__
    __atomic_store(p, &v, __ATOMIC_SEQ_CST);
#else
    *p = v;
#endif
}
