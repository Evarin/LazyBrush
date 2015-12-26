#define min(a,b) (a<b ? a : b)
#define ZERO_CAP 0.001f
#define tree_cap(p,e) (p->tree == -1 ? e : e->symmetric)->capacity

typedef struct _node {
  // Itération
  int _mark_connected_itn;

  // Marqueurs booléens
  char disabled, active, orphan, tree, origin;

  // Couleur, si précisée dès le départ, pour lazybrush
  char color;

  // Label, pour dfs
  char label;

  // Parent dans l'arbre (pour mincut)
  struct _edge *parent;

  // Voisins (liste)
  struct _edge *neighbour;

  // Structures de liste pour mincut
  // Active -> doublement chaîné pour suppression d'élément quelconque
  struct _node *_prev_active, *_next_active, *_next_orphan;

  // Structure de liste pour lazybrush
  struct _node *_next_in_group;
} Node;

typedef struct _edge {
  // Noeuds reliés
  Node *a, *b;

  // Capacité
  float capacity, orig_capacity;

  // Lien symétrique b->a
  struct _edge *symmetric;

  // Structure de liste pour exploration de voisins
  // Doublement chainé pour suppression d'élément quelconque
  struct _edge *_next_edge, *_prev_edge;

  // Structure de liste pour construction de chemin
  struct _edge *_next_on_path;
} Edge;

typedef struct _graph {
  struct _node *first_node;
} Graph;

typedef struct _queue {
  // Double queue pour les noeuds actifs dans mincut
  Node *top1, *top2, *bottom1, *bottom2;
} Queue;

// Initialisations
void reinit_node(Node *node) {
  node->parent = NULL;
  node->_next_active = NULL;
  node->_prev_active = NULL;
  node->_next_orphan = NULL;
  node->_mark_connected_itn = -1;
  node->active = 0;
  node->orphan = 0;
  node->tree = 0;
  node->label = 0;
}

void init_node(Node *node, Node *next) {
  node->neighbour = NULL;
  node->disabled = 0;
  node->origin = 0;
  node->color = 0;
  node->_next_in_group = NULL;
  reinit_node(node);
}

void reinit_edge(Edge *e) {
  e->capacity = e->orig_capacity;
  e->_next_on_path = NULL;
}

Edge* connect_nodes(Node *a, Node *b, const float capacity, Edge *symmetric){
  Edge *e = (Edge *)malloc(sizeof(Edge));
  e->a = a;
  e->b = b;
  e->capacity = capacity;
  e->orig_capacity = capacity;
  e->_next_edge = a->neighbour;
  if(a->neighbour) a->neighbour->_prev_edge = e;
  e->_prev_edge = NULL;
  a->neighbour = e;
  e->symmetric = symmetric;
  if(symmetric!=NULL) symmetric->symmetric = e;
  return e;
}

Node *spawn_nodes(Graph *g, int k) {
  Node *nodes = (Node *)malloc(k*sizeof(Node));
  Node *last_node = nodes + k - 1;
  Node *n;
  for(n = nodes; n<last_node; n++) {
    init_node(n, n+1);
  }
  init_node(last_node, g->first_node);
  g->first_node = nodes;
  return nodes;
}

Graph *make_graph(void) {
  Graph *g = (Graph *)malloc(sizeof(Graph)); 
  g->first_node = NULL;
  return g;
}

void full_reinit_node(Node *node) {
  reinit_node(node);
  Edge *e;
  for(e = node->neighbour; e!=NULL; e = e->_next_edge)
    reinit_edge(e);
}

void init_queue(Queue *q) {
  q->top1 = q->top2 = q->bottom1 = q->bottom2 = NULL;
}

// Mises à jour

void set_node_parent(Node *n, Edge *e) {
  n->parent = e;
}

void remove_edge(Edge *e) {
  if(e->_prev_edge)
    e->_prev_edge->_next_edge = e->_next_edge;
  else
    e->a->neighbour = e->_next_edge;
  if(e->_next_edge)
    e->_next_edge->_prev_edge = e->_prev_edge;
  if(e->symmetric) e->symmetric->symmetric = NULL; // ?
  free(e);
}

void disable_node(Node *n) {
  n->disabled = 1;
  while(n->neighbour){
    if(n->neighbour->symmetric) remove_edge(n->neighbour->symmetric);
    remove_edge(n->neighbour);
  }
}

void enable_node(Node *n) {
  n->disabled = 0;
}

Node *get_active(Queue *actives) {
  Node *p = actives->top1;
  if(!p) {
    actives->top1 = actives->top2;
    actives->bottom1 = actives->bottom2;
    actives->top2 = NULL;
    actives->bottom2 = NULL;
  }
  p = actives->top1;
  if(p && !p->active) printf("ASSERT FALSE%p,%p", p, p->_next_active);
  return p;
}

void push_active(Queue *actives, Node *p) {
  if(p->active) { return; }
  p->_prev_active = actives->bottom2;
  if(actives->bottom2) actives->bottom2->_next_active = p;
  else actives->top2 = p;

  p->_next_active = NULL;
  p->active = 1;
  actives->bottom2 = p;
}

void remove_active(Queue *actives, Node *p) {
  if(!p->active) return;
  if(p->_prev_active)
    p->_prev_active->_next_active = p->_next_active;
  if(p->_next_active)
    p->_next_active->_prev_active = p->_prev_active;
  if(actives->top1 == p)
    actives->top1 = p->_next_active;
  if(actives->bottom1 == p)
    actives->bottom1 = p->_prev_active;
  if(actives->top2 == p)
    actives->top2 = p->_next_active;
  if(actives->bottom2 == p)
    actives->bottom2 = p->_prev_active;

  p->_prev_active = p->_next_active = NULL;
  p->active = 0;
}

Node *pop_orphan(Node **orphans) {
  Node *p = *orphans;
  *orphans = p->_next_orphan;
  p->_next_orphan = NULL;
  p->orphan = 0;
  return p;
}

void push_orphan(Node **orphans, Node *p) {
  if(p->_next_orphan) return; // ASSERT FALSE
  p->_next_orphan = *orphans;
  p->orphan = 1;
  p->_mark_connected_itn = -1;
  *orphans = p;
}

// Sous routines

Edge* make_path(Edge *e, float *Delta) {
  if(e->a->tree != -1) { // == s->tree;
    e = e->symmetric;
    if(!e) return NULL; // ASSERT FALSE
  }
  Edge *f = e, *g;
  float min_cap = e->capacity;
  Node *p = e->b;
  // On descend le courant
  while(p->parent) {
    g = p->parent;
    p = p->parent->b;

    f->_next_on_path = g;
    min_cap = min(min_cap, g->capacity);
    f = g;
  }
  f->_next_on_path = NULL;
  // On le remonte
  f = e;
  p = e->a;
  while(p->parent) {
    g = p->parent->symmetric;
    p = p->parent->b;

    if(!g) return NULL; // ASSERT FALSE
    g->_next_on_path = f;
    min_cap = min(min_cap, g->capacity);
    f = g;
  }
  *Delta = min_cap;
  return f;
}

char connected_to_origin(Edge *e, const int itn){
  if(e==NULL) return 0;
  Edge *f = e;
  while(f->a->_mark_connected_itn != itn && f->b->parent) {
    f = f->b->parent;
  }
  if(f->b->origin || f->a->_mark_connected_itn == itn) {
    f = e;
    while(f->a->_mark_connected_itn != itn && f->b->parent) {
      f->a->_mark_connected_itn = itn;
      f = f->b->parent;
    }
    return 1;
  }
  return 0;
}

// Etapes de l'algorithme

Edge* grow(Queue *actives) {
  Node *p, *q;
  while((p = get_active(actives))) {
    Edge *e; // edge p->q
    for(e = p->neighbour; e != NULL; e = e->_next_edge) {
      if(tree_cap(p,e) <= ZERO_CAP) {
	continue;
      }
      q = e->b;
      if(q->tree == 0) {
	q->tree = p->tree;
	q->parent = e->symmetric;
	if(!q->parent) { printf("Assert false"); return NULL; } // ASSERT FALSE
	push_active(actives, q);
      } else if(q->tree != p->tree) {
	return e;
      }
    }
    remove_active(actives, p);
  }
  return NULL;
}

void augment(Edge *f, Node **orphans) {
  float Delta;
  Edge *pathstart = make_path(f, &Delta);
  Edge *e;
  for(e = pathstart; e != NULL; e = e->_next_on_path){
    e->capacity -= Delta;
    if(e->capacity <= ZERO_CAP) {
      if(e->a->tree != e->b->tree) continue;
      if(e->a->tree == -1) {
	e->b->parent = NULL;
	push_orphan(orphans, e->b);
	continue;
      }
      if(e->a->tree == 1) {
	e->a->parent = NULL;
	push_orphan(orphans, e->a);
      }
    }
  }
}

void adopt(Queue *actives, Node **orphans, const int itn) {
  while(*orphans) {
    Node *p = pop_orphan(orphans), *q;
    Edge *e;
    Edge *new_parent = NULL;
    for(e = p->neighbour; e != NULL; e = e->_next_edge) {
      q = e->b;
      if(q->tree != p->tree) continue;
      if(tree_cap(q, e->symmetric) <= ZERO_CAP) continue;
      if(!connected_to_origin(q->parent, itn)) continue;
      new_parent = e;
      break;
    }
    if(new_parent) {
      p->parent = new_parent;
    } else {
      for(e = p->neighbour; e != NULL; e = e->_next_edge) {
	q = e->b;
	if(q->tree != p->tree) continue;
	if(tree_cap(q, e->symmetric) > ZERO_CAP)
	  push_active(actives, e->b);
	if(q->parent && q->parent->b == p) {
	  push_orphan(orphans, q);
	  q->parent = NULL;
	}
      }
      p->tree = 0;
      remove_active(actives, p);
    }
  }
}

void mincut(Graph *g, Node *s, Node *t) {
  printf("MINCUT");
  Queue actives;
  init_queue(&actives);
  Node *orphans = NULL;
  push_active(&actives, t);
  push_active(&actives, s);
  s->tree = -1;
  t->tree = 1;
  s->origin = t->origin = 1;
  int i = 0;
  while(1) {
    // Growth
    Edge *contact = grow(&actives);
    if(contact == NULL) return;
    // Augmentation
    augment(contact, &orphans);
    // Adoption
    adopt(&actives, &orphans, i);
    i++;
  }
}


// Util : DFS, uses the "orphans" structure

char dfs(Node *p, const char label, const char ctoset) {
  Edge *e;
  Node *q, *top = p;
  char onecolor = 1, color = 0;
  p->label = label;
  while((p = top)) {
    top = top->_next_orphan;
    p->_next_orphan = NULL;
    if(p->color) {
      if(color) {
	if(p->color != color) onecolor = 0;
      } else
	color = p->color;
    }
    for(e = p->neighbour; e != NULL; e = e->_next_edge) {
      q = e->b;
      if(q->disabled || q->label == label) continue;
      q->label = label;
      q->_next_orphan = top;
      top = q;
    }
    if(ctoset) {
      disable_node(p);
      p->color = ctoset;
    }
  }
  return (onecolor ? color : 0);
}
