#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <Python.h>
#include <numpy/arrayobject.h>
#include "mincut.c"


npy_intp explore_regions(Node *points, npy_intp length, char *colornames, 
			 Node **colorsets, npy_intp mincol, npy_intp numcols) {

  printf("EXPLORE\n");
  npy_intp k;
  Node *p, *q;
  for(k=0; k<length; k++) {
    p = points+k;
    if(p->disabled || p->label) continue;

    // Exlore la région à la recherche de plusieurs couleurs
    int color = dfs(p, 1, 0);
    printf(" Couleur %d\n", color);

    // Si une seule -> on colorie la région et on désactive les points
    if(color) {
      printf("Région monochrome %d\n", (int)color);
      dfs(p, 2, color);

      // On cherche l'existence de cette couleur dans d'autres régions actives
      char stillalive = 0;
      for(q = colorsets[color-1]; q != NULL; q = q->_next_in_group)
	if(!q->disabled) {
	  stillalive = 1;
	  break;
	}

      // Si non : on supprime la couleur
      if(!stillalive) {
	printf("Suppression de cette couleur\n");
	colornames[color] = colornames[mincol];
	mincol++;
      }
    }
  }
  return mincol;
}

static PyObject *
lazybrush_wrapper(PyObject *self, PyObject *args)
{

  // Lecture des paramètres
  PyArrayObject *sketch=NULL, *colors=NULL, *colorlist=NULL, *output=NULL;
  float K, lambda;

  if (!PyArg_ParseTuple(args, "O!O!O!ffO!", 
			&PyArray_Type, &sketch, &PyArray_Type, &colors, 
			&PyArray_Type, &colorlist,
			&K, &lambda, 
			&PyArray_Type, &output)) return NULL;

  int s_nd = PyArray_NDIM(sketch), c_nd = PyArray_NDIM(colors), 
    o_nd = PyArray_NDIM(output), cl_nd = PyArray_NDIM(colorlist);
  npy_intp *s_dims = PyArray_DIMS(sketch), *c_dims = PyArray_DIMS(colors), *o_dims = PyArray_DIMS(output);
  npy_intp wdt = s_dims[0], hgt = s_dims[1];
  if (s_nd != 2 || c_nd != 1 || o_nd != 1 || cl_nd != 1
      || c_dims[0] != wdt * hgt || o_dims[0] != c_dims[0]) {
    PyErr_SetString(PyExc_ValueError,
		    "Paramètres invalides");
    return NULL;
  }

  Py_DECREF(c_dims);
  Py_DECREF(s_dims);
  Py_DECREF(o_dims);

  // Initialisation des structures de graphe
  printf("Dimensions : %dx%d\n", (int)wdt, (int)hgt);

  Graph *graph = make_graph();
  Node *points = spawn_nodes(graph, wdt*hgt);
  Node *points_end = points + wdt*hgt;

  npy_intp i, j;
  npy_intp numcols = PyArray_DIMS(colorlist)[0];

  Node **colorsets = (Node **)malloc((numcols-1)*sizeof(Node **));
  char *colornames = (char *)malloc(numcols*sizeof(char));
  colornames[0] = 0; // Transparent -> ne doit pas rester à la fin
  for(i=0;i<numcols-1;i++){
    colorsets[i] = NULL;
    colornames[i+1] = i+1;
  }

  for(j=0; j<hgt; j++) {
    for(i=0; i<wdt; i++) {
      Node *p = points+i+j*wdt, *q;
      Edge *e;
      float cp = *((double *)PyArray_GETPTR2(sketch, i, j)), cq;
      if (i < wdt-1) {
	cq = *((double *)PyArray_GETPTR2(sketch, i+1, j));
	q = p+1;
	e = connect_nodes(p, q, K * cp + 1.0f, NULL);
	connect_nodes(q, p, K * cq + 1.0f, e);
      }
      if (j < hgt-1) {
	q = p+wdt;
	cq = *((double *)PyArray_GETPTR2(sketch, i, j+1));
	e = connect_nodes(p, q, K * cp + 1.0f, NULL);
	connect_nodes(q, p, K * cq + 1.0f, e);
      }
      npy_intp c = *((npy_intp *)PyArray_GETPTR1(colors, i+j*wdt));
      if(c>0) {
	p->_next_in_group = colorsets[c-1];
	p->color = c;
	colorsets[c-1] = p;
      }
    }
  }
 

  npy_intp c = 1, k;
  char color = 1;
  Node *s = spawn_nodes(graph, 2);
  Node *t = s+1;
  s->origin = t->origin = 1;
  Node *p;

  // Algorithme
  printf("%d Couleurs\n", (int)numcols);
  while(c < numcols) {
    // Exploration des régions à la recherche de monochromes
    c = explore_regions(points, wdt*hgt, colornames, 
			    colorsets, c, numcols);
    color = colornames[c];

    if(c >= numcols-1) break;
    // Fabrication du graphe
    printf("Traitement couleur %d (%d)\n", (int)color, (int)c);
    // On relie les points de couleur c à S
    for (p = colorsets[color-1]; p != NULL; p = p->_next_in_group) {
      if(p->disabled) continue;
      Edge *e = connect_nodes(s, p, (1.0f-lambda)*K, NULL);
      connect_nodes(p, s, 0.0f, e);
    }
    int d;
    npy_intp acolor;
    // On relie les points de couleur non encore traitée à T
    for (d = color+1; d<numcols; d++) {
      acolor = colornames[d];
      for(p = colorsets[acolor-1]; p != NULL; p = p->_next_in_group) {
	if(p->disabled) continue;
	Edge *e = connect_nodes(p, t, (1.0f-lambda)*K, NULL);
	connect_nodes(t, p, 0.0f, e);
      }
    }

    // Mincut
    mincut(graph, s, t);

    printf("\n\n");
    // On assigne la couleur à ce qui a été relié à S
    for(k = 0; k < wdt*hgt; k++) {
      p = points+k;
      if(p->disabled) continue;
      if(p->tree == -1) {
	p->color = color;
	disable_node(p);
      }// else printf("%d", p->tree);
      if(p->tree == 0) { // just debug
	p->color = 0;
	disable_node(p);
      }
    }
    
    // On efface la couleur des résidus
    for (p = colorsets[color-1]; p != NULL; p = p->_next_in_group) {
      if(p->disabled) continue;
      p->color = 0;
    }

    // On réinitialise le graphe
    disable_node(s);
    disable_node(t);
    for(p = points; p < points_end; p++)
      if(!(p->disabled)) full_reinit_node(p);
    reinit_node(s);
    reinit_node(t);
    enable_node(s); 
    enable_node(t);
    // On désactive la couleur
    c++;
  }

  for(k = 0; k < wdt*hgt; k++) {
    p = points+k;
    *((npy_intp *)PyArray_GETPTR1(output, k)) = p->color;
  }
  

  // Nettoyage
  free(s);
  free(graph);
  free(points);

  Py_INCREF(Py_None);
  return Py_None;

  //fail:
  //return NULL;
}

static PyMethodDef LazybrushMethods[] = {
  {"wrapper", lazybrush_wrapper, METH_VARARGS,
   "Computes lazybrush"},
  {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
initlazybrush(void) {
  (void) Py_InitModule("lazybrush", LazybrushMethods);
  import_array();
}

int 
main(int argc, char *argv[]) {
  /* Pass argv[0] to the Python interpreter */
  Py_SetProgramName(argv[0]);

  /* Initialize the Python interpreter.  Required. */
  Py_Initialize();

  /* Add a static module */
  initlazybrush();
    
  return 0;
}
