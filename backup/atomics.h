template <class T> static T atomic_load_weak(volatile T *p) {
    T v;
    __atomic_load(p, &v, __ATOMIC_RELAXED);
    return v;
}
template <class T> static T atomic_load_strong(volatile T *p) {
    T v;
    __atomic_load(p, &v, __ATOMIC_SEQ_CST);
    return v;
}

template <class T> static T atomic_store_weak(volatile T *p, T v) {
    __atomic_store(p, &v, __ATOMIC_SEQ_CST);
    return v;
}

template <class T> static T atomic_store_strong(volatile T *p, T v) {
    __atomic_store(p, &v, __ATOMIC_SEQ_CST);
    return v;
}
