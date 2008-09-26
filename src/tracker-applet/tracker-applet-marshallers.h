#ifndef __tracker_MARSHAL_H__
#define __tracker_MARSHAL_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* VOID:STRING,BOOLEAN,BOOLEAN,BOOLEAN,BOOLEAN,BOOLEAN (/dev/stdin:1) */
extern void tracker_VOID__STRING_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN (GClosure     *closure,
									  GValue       *return_value,
									  guint		n_param_values,
									  const GValue *param_values,
									  gpointer	invocation_hint,
									  gpointer	marshal_data);

/* VOID:INT (/dev/stdin:2) */
#define tracker_VOID__INT	g_cclosure_marshal_VOID__INT

/* VOID:STRING,STRING,INT,INT,INT (/dev/stdin:3) */
extern void tracker_VOID__STRING_STRING_INT_INT_INT_DOUBLE (GClosure	 *closure,
							    GValue	 *return_value,
							    guint	  n_param_values,
							    const GValue *param_values,
							    gpointer	  invocation_hint,
							    gpointer	  marshal_data);

G_END_DECLS

#endif /* __tracker_MARSHAL_H__ */

