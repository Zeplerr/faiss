/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef LOCKS_LEVELS_INCLUDED
#define LOCKS_LEVELS_INCLUDED
#include <faiss/utils/utils.h>
#include <pthread.h>
#include <unordered_map>
#include <unordered_set>

/**********************************************
 * LockLevels
 **********************************************/
struct LockLevels {
    /* There n times lock1(n), one lock2 and one lock3
     * Invariants:
     *    a single thread can hold one lock1(n) for some n
     *    a single thread can hold lock2, if it holds lock1(n) for some n
     *    a single thread can hold lock3, if it holds lock1(n) for some n
     *       AND lock2 AND no other thread holds lock1(m) for m != n
     */
    pthread_mutex_t mutex1;
    pthread_cond_t level0_cv;
    pthread_cond_t level1_cv;
    pthread_cond_t level2_cv;
    pthread_cond_t level3_cv;

    std::unordered_set<int> level1_holders;      // which level1 locks are held
    std::unordered_map<int, int> level0_holders; // which level0 locks are held
    int n_level2;       // nb threads that wait on level2
    bool level3_in_use; // a threads waits on level3
    bool level2_in_use;
    LockLevels();
    ~LockLevels();
    void lock_0(int no);
    void unlock_0(int no);
    void lock_1(int no);
    void unlock_1(int no);
    void lock_2();
    void unlock_2();
    void lock_3();
    void unlock_3();
    void print();
};
#endif