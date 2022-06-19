typedef enum
{
  CONNECT,
  INITIATE,
  TEST_MSG,
  ACCEPT,
  REJECT,
  REPORT_MSG,
  CHANGE_ROOT,
  WAKEUP
} message;

typedef enum
{
  SLEEPING,
  FIND,
  FOUND
} node_state;

typedef enum
{
  E_BASIC,
  E_BRANCH,
  E_REJECT
} edge_state;

typedef struct node node;

typedef struct edge
{
  int weight;
  int node;
} edge;

typedef struct node
{
  node_state state;
  int ind;
  int frag;
  int* edge_states;
  edge* edges;
  int num_edges;
  int level;
  int best_edge;
  int best_weight;
  int test_edge;
  int parent;
  int find_count;
} node;

void init(int num_edges, int* edges, int* weights, node* nodes);
void read_message( );
int find_min_edge( );
void wake_up( );
void _connect(int a, int b);
void initiate(int a, int b, int c, int d);
void test( );
void test_message(int a, int b, int c);
void accept(int a);
void reject(int a);
void report( );
void report_message(int a, int b);
void change_root( );

