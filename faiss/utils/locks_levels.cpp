/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <faiss/utils/locks_levels.h>

/**********************************************
 * LockLevels
 **********************************************/

LockLevels::LockLevels() {
    pthread_mutex_init(&mutex1, nullptr);
    pthread_cond_init(&level0_cv, nullptr);
    pthread_cond_init(&level1_cv, nullptr);
    pthread_cond_init(&level2_cv, nullptr);
    pthread_cond_init(&level3_cv, nullptr);
    n_level2 = 0;
    level2_in_use = false;
    level3_in_use = false;
}

LockLevels::~LockLevels() {
    pthread_cond_destroy(&level1_cv);
    pthread_cond_destroy(&level2_cv);
    pthread_cond_destroy(&level3_cv);
    pthread_mutex_destroy(&mutex1);
}

void LockLevels::lock_0(int no) {
    pthread_mutex_lock(&mutex1);
    while (level3_in_use || level1_holders.count(no) > 0) {
        pthread_cond_wait(&level0_cv, &mutex1);
    }
    if (level0_holders.count(no) == 1) {
        level0_holders[no]++;
    } else {
        level0_holders[no] = 1;
    }
    pthread_mutex_unlock(&mutex1);
}

void LockLevels::unlock_0(int no) {
    pthread_mutex_lock(&mutex1);
    assert(level0_holders.count(no) == 1);
    if (level0_holders[no] == 0) {
        goto exit;
    }
    level0_holders[no]--;
    assert(level0_holders[no] >= 0);
exit:
    pthread_cond_broadcast(&level1_cv);
    pthread_mutex_unlock(&mutex1);
}

void LockLevels::lock_1(int no) {
    pthread_mutex_lock(&mutex1);
    while (level3_in_use || level1_holders.count(no) > 0 ||
           (level0_holders.count(no) > 0 && level0_holders[no] > 0)) {
        pthread_cond_wait(&level1_cv, &mutex1);
    }
    level1_holders.insert(no);
    pthread_mutex_unlock(&mutex1);
}

void LockLevels::unlock_1(int no) {
    pthread_mutex_lock(&mutex1);
    assert(level1_holders.count(no) == 1);
    level1_holders.erase(no);
    if (level3_in_use) { // a writer is waiting
        pthread_cond_signal(&level3_cv);
    } else {
        pthread_cond_broadcast(&level1_cv);
    }
    pthread_cond_broadcast(&level0_cv);
    pthread_mutex_unlock(&mutex1);
}

void LockLevels::lock_2() {
    pthread_mutex_lock(&mutex1);
    n_level2++;
    if (level3_in_use) { // tell waiting level3 that we are blocked
        pthread_cond_signal(&level3_cv);
    }
    while (level2_in_use) {
        pthread_cond_wait(&level2_cv, &mutex1);
    }
    level2_in_use = true;
    pthread_mutex_unlock(&mutex1);
}

void LockLevels::unlock_2() {
    pthread_mutex_lock(&mutex1);
    level2_in_use = false;
    n_level2--;
    pthread_cond_signal(&level2_cv);
    pthread_mutex_unlock(&mutex1);
}

void LockLevels::lock_3() {
    pthread_mutex_lock(&mutex1);
    level3_in_use = true;
    // wait until there are no level1 holders anymore except the
    // ones that are waiting on level2 (we are holding lock2)
    while (level1_holders.size() > n_level2) {
        pthread_cond_wait(&level3_cv, &mutex1);
    }
    // don't release the lock!
}

void LockLevels::unlock_3() {
    level3_in_use = false;
    // wake up all level1_holders
    pthread_cond_broadcast(&level1_cv);
    pthread_mutex_unlock(&mutex1);
}

void LockLevels::print() {
    pthread_mutex_lock(&mutex1);
    printf("State: level3_in_use=%d n_level2=%d level1_holders: [",
           level3_in_use,
           n_level2);
    for (int k : level1_holders) {
        printf("%d ", k);
    }
    printf("]\nlevel0_holders:[");
    for (auto pairs : level0_holders) {
        printf("%d:%d, ", pairs.first, pairs.second);
    }
    printf("]\n");
    pthread_mutex_unlock(&mutex1);
}