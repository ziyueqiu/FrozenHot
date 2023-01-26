#ifndef _COMMON_WORKSPACE_H_
#define _COMMON_WORKSPACE_H_

namespace FastHash
{
    typedef unsigned TKey;

    template <typename TValue> 
    class FashHashAPI {
    public:
        virtual bool insert(TKey idx, TValue value) = 0;

        virtual bool find(TKey idx, TValue &value) = 0;

        virtual void thread_init(int tid) {}

        virtual void clear() = 0;
    };
    
} // namespace FastHash

#endif // _COMMON_WORKSPACE_H_