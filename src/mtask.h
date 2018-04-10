 /****************************************************************************
 *                                                                           *
 *                                                      ,8                   *
 *                                                    ,d8*                   *
 *                                                  ,d88**                   *
 *                                                ,d888`**                   *
 *                                             ,d888`****                    *
 *                                            ,d88`******                    *
 *                                          ,d88`*********                   *
 *                                         ,d8`************                  *
 *                                       ,d8****************                 *
 *                                     ,d88**************..d**`              *
 *                                   ,d88`*********..d8*`****                *
 *                                 ,d888`****..d8P*`********                 *
 *                         .     ,d8888*8888*`*************                  *
 *                       ,*     ,88888*8P*****************                   *
 *                     ,*      d888888*8b.****************                   *
 *                   ,P       dP  *888.*888b.**************                  *
 *                 ,8*        8    *888*8`**88888b.*********                 *
 *               ,dP                *88 8b.*******88b.******                 *
 *              d8`                  *8b 8b.***********8b.***                *
 *            ,d8`                    *8. 8b.**************88b.              *
 *           d8P                       88.*8b.***************                *
 *         ,88P                        *88**8b.************                  *
 *        d888*       .d88P            888***8b.*********                    *
 *       d8888b..d888888*              888****8b.*******        *            *
 *     ,888888888888888b.              888*****8b.*****         8            *
 *    ,8*;88888P*****788888888ba.      888******8b.****      * 8' *          *
 *   ,8;,8888*         `88888*         d88*******8b.***       *8*'           *
 *   )8e888*          ,88888be.       888*********8b.**       8'             *
 *  ,d888`           ,8888888***     d888**********88b.*    d8'              *
 * ,d88P`           ,8888888Pb.     d888`***********888b.  d8'               *
 * 888*            ,88888888**   .d8888*************      d8'                *
 * `88            ,888888888    .d88888b*********        d88'                *
 *               ,8888888888bd888888********             d88'                *
 *               d888888888888d888********                d88'               *
 *               8888888888888888b.****                    d88'              *
 *               88*. *88888888888b.*      .oo.             d888'            *
 *               `888b.`8888888888888b. .d8888P               d888'          *
 *                **88b.`*8888888888888888888888b...            d888'        *
 *                 *888b.`*8888888888P***7888888888888e.          d888'      *
 *                  88888b.`********.d8888b**  `88888P*            d888'     *
 *                  `888888b     .d88888888888**  `8888.            d888'    *
 *                   )888888.   d888888888888P      `8888888b.      d88888'  *
 *                  ,88888*    d88888888888**`        `8888b          d888'  *
 *                 ,8888*    .8888888888P`              `888b.        d888'  *
 *                ,888*      888888888b...                `88P88b.  d8888'   *
 *       .db.   ,d88*        88888888888888b                `88888888888     *
 *   ,d888888b.8888`         `*888888888888888888P`              ******      *
 *  /*****8888b**`              `***8888P*``8888`                            *
 *   /**88`                               /**88`                             *
 *   `|'                             `|*8888888'                             *
 *                                                                           *
 * mtask.h                                                                   *
 *                                                                           *
 * Do rudimentary multitasking.                                              *
 * This scheduler implements cooperative multitasking.                       *
 * Context switching occurs only on request.                                 *
 *                                                                           *
 * Copyright (c) 2017 Ayron                                                  *
 *                                                                           *
 *****************************************************************************
 *                                                                           *
 *  This program is free software; you can redistribute it and/or modify     *
 *  it under the terms of the GNU General Public License as published by     *
 *  the Free Software Foundation; either version 2 of the License, or        *
 *  (at your option) any later version.                                      *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License along  *
 *  with this program; if not, write to the Free Software Foundation, Inc.,  *
 *  51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.                  *
 *                                                                           *
 ****************************************************************************/

#ifndef MTASK_H_
#define MTASK_H_

enum triggers {
	TRIGGER_NONE = 0,
	TRIGGER_USB = 1,
	TRIGGER_DATA = 2,
	TRIGGER_TIMEOUT = 4,
	TRIGGER_NETWORK = 8,
	TRIGGER_SPI = 16
};

/* network contexts */
#define CONTEXT_NETWORK_TXRDY	1
#define CONTEXT_NETWORK_RIPRCV	2

void mtask_init(void);
enum triggers coroutine_yield(enum triggers trigger, int context);
void coroutine_abort(void);
void coroutine_trigger(enum triggers trigger, int context);

int coroutine_invoke_later(void (*func) (unsigned long), unsigned long param, const char *name);
int coroutine_invoke_urgent(void (*func) (unsigned long), unsigned long param, const char *name);

#endif				/* MTASK_H_ */
