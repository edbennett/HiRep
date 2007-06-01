#ifdef _TRADITIONAL_STYLE_

#define _MINDEX_ "(" << a.row << "," << a.col << ")"
#define _VINDEX_ "[" << a.row << "]"
#define _INDENT_ "\t"
#define _ENDL_ ";\n"
#define _LASTENDL_ ";\n"

#endif

#ifdef _PATELLA_STYLE_

#define _MINDEX_ "[" << a.row*a.size+a.col << "]"
#define _VINDEX_ "[" << a.row << "]"
#define _INDENT_ "\t"
#define _ENDL_ ";\n"
#define _LASTENDL_ ";\n"

#endif

#ifdef _PICA_STYLE_

#define _MINDEX_ ".c" << (a.row+1) << "_" << (a.col+1)
#define _VINDEX_ ".c" << (a.row+1)
#define _INDENT_ "\t"
#define _ENDL_ "; \\\n"
#define _LASTENDL_ ";\n"
#define _GROUP_MATRIX_ "suNg_matrix"
#define _ALGEBRA_VECTOR_ "suNg_algebra_vector"
#define _REPR_MATRIX_ "suNf_matrix"
#define _REPR_VECTOR_ "suNf_vector"

#endif