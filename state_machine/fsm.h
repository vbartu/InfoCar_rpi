/*
 * fsm.h
 *
 *  Created on: 1 de mar. de 2016
 *      Author: Administrador
 */

#ifndef FSM_H_
#define FSM_H_

typedef struct fsm_t fsm_t;

typedef int (*fsm_input_func_t) (fsm_t*);
typedef void (*fsm_output_func_t) (fsm_t*);

typedef struct fsm_trans_t {
  int orig_state;
  fsm_input_func_t in;
  int dest_state;
  fsm_output_func_t out;
} fsm_trans_t;

struct fsm_t {
  int current_state;
  fsm_trans_t* tt;
  void* user_data;
};

fsm_t* fsm_new (int state, fsm_trans_t* tt, void* user_data);
void fsm_init (fsm_t* this, int state, fsm_trans_t* tt, void* user_data);
void fsm_fire (fsm_t* this);
void fsm_destroy (fsm_t* this);

#endif /* FSM_H_ */
