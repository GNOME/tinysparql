#ifndef __MINERS_MOCK_H__
#define __MINERS_MOCK_H__

#include <glib.h>

G_BEGIN_DECLS

#define MOCK_MINER_1 "org.freedesktop.Tracker1.Miner.Mock1"
#define MOCK_MINER_2 "org.freedesktop.Tracker1.Miner.Mock2"

/*
 * Assumptions:
 *
 *  There are this two miners, 
 *  Initial state: Mock1 is running, Mock2 is paused
 *
 */
void    miners_mock_init (void);

G_END_DECLS


#endif
