
#define EQ(x)                                                                  \
  if (ps.z) {                                                                  \
    x;                                                                         \
  }

#define NE(x)                                                                  \
  if (!ps.z) {                                                                 \
    x;                                                                         \
  }

#define CS(x)                                                                  \
  if (ps.cs) {                                                                 \
    x;                                                                         \
  }

#define CC(x)                                                                  \
  if (!ps.cs) {                                                                \
    x;                                                                         \
  }

#define MI(x)                                                                  \
  if (ps.mi) {                                                                 \
    x;                                                                         \
  }

#define PL(x)                                                                  \
  if (!ps.mi) {                                                                \
    x;                                                                         \
  }

#define VS(x)                                                                  \
  if (ps.vs) {                                                                 \
    x;                                                                         \
  }

#define VC(x)                                                                  \
  if (!ps.vs) {                                                                \
    x;                                                                         \
  }

#define HI(x)                                                                  \
  if (ps.cs && !ps.z) {                                                        \
    x;                                                                         \
  }

#define LS(x)                                                                  \
  if (!ps.cs || ps.z) {                                                        \
    x;                                                                         \
  }

#define GE(x)                                                                  \
  if (ps.mi == ps.vs) {                                                        \
    x;                                                                         \
  }

#define LT(x)                                                                  \
  if (ps.mi != ps.vs) {                                                        \
    x;                                                                         \
  }

#define GT(x)                                                                  \
  if (!ps.z && (ps.mi == ps.vs)) {                                             \
    x;                                                                         \
  }

#define LE(x)                                                                  \
  if (ps.z || (ps.mi != ps.vs)) {                                              \
    x;                                                                         \
  }

#define AL(x)                                                                  \
  if (1) {                                                                     \
    x;                                                                         \
  }

#define NV(x)                                                                  \
  if (0) {                                                                     \
    x;                                                                         \
  }
