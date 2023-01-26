#include "FastHashBase.h"
#include "clht.h"

namespace FastHash
{
    template <typename TValue>
    class CLHT :public FashHashAPI<TValue> {
    private:
	    clht_t* m_hashtable;
        uint32_t m_buckets;

    public:
	CLHT(int size, int exp) {
            int ex = 0;
            while (size != 0) {
                ex++;
                size >>= 1;
            }
	    m_buckets = (uint32_t)(1 << (ex + exp));
	    printf ("Number of buckets: %u\n", m_buckets);
            m_hashtable = clht_create(m_buckets);
	};

	~CLHT() { 
	    clht_gc_destroy(m_hashtable);
	}

    virtual void thread_init(int tid) {
	    clht_gc_thread_init(m_hashtable, (uint32_t)tid);
	}

	virtual bool insert(TKey idx, TValue value) {
           return clht_put(m_hashtable, (clht_addr_t)idx, (clht_val_t)(value.get())) != 0;
	};

	virtual bool find(TKey idx, TValue &value) {
	   clht_val_t v = clht_get(m_hashtable->ht, (clht_addr_t)idx);
	   if (v == 0) return false;
	   value.reset((std::string*)v, [](auto p) {});
	   return true;
	};

	virtual void clear() {
	    clht_clear(m_hashtable->ht);
	};
    };

} // namespace FastHash