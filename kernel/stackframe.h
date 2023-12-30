


typedef uint64* sf_t;

#define RETADDR(sf) (*(sf - 1))

#define CALLER_SF(sf) ((sf_t)*(sf - 2))
