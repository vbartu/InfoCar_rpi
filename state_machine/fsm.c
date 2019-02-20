/*
 * fsm.c
 *
 *  Created on: 1 de mar. de 2016
 *      Author: Administrador
 */

#include <stdlib.h>
#include "fsm.h"

fsm_t*
fsm_new (int state, fsm_trans_t* tt, void* user_data)
{
  fsm_t* this = (fsm_t*) malloc (sizeof (fsm_t));
  fsm_init (this, state, tt, user_data);
  return this;
}

void
fsm_init (fsm_t* this, int state, fsm_trans_t* tt, void* user_data)
{
  this->current_state = state;
  this->tt = tt;
  this->user_data = user_data;
}

void
fsm_destroy (fsm_t* this)
{
  free(this);
}

void
fsm_fire (fsm_t* this)
{
  fsm_trans_t* t;
  for (t = this->tt; t->orig_state >= 0; ++t) {
    if ((this->current_state == t->orig_state) && t->in(this)) {
      this->current_state = t->dest_state;
      if (t->out)
        t->out(this);
      break;
    }
  }
}

