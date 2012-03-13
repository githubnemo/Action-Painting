#ifndef __STATE_H__
#define __STATE_H__

enum State {STATE_SEARCHING, STATE_NEW_USER, STATE_SETUP};

void SetState(State s);
State GetState();

#endif
