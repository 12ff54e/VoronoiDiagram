#ifndef VD_COUNTER_
#define VD_COUNTER_

template <typename T>
class Counter {
   private:
    static inline unsigned _count = 0;

   public:
    Counter() { ++_count; }
    Counter(const Counter&) : Counter() {}
    Counter(Counter&&) : Counter() {}
    static unsigned get_count() { return _count; }

   protected:
    ~Counter() { --_count; }
};

#endif  // VD_COUNTER_
