#ifndef _SYNCHRONIZED_QUEUE_H_
#define _SYNCHRONIZED_QUEUE_H_

#include <pthread.h>
#include <queue>

template <typename T>
class SynchronizedQueue
{
public:

	SynchronizedQueue()
	{
		pthread_mutex_init(&mMutex, NULL);
        pthread_cond_init(&mCondv, NULL);
	}
	
	~SynchronizedQueue() 
	{
		pthread_mutex_destroy(&mMutex);
		pthread_cond_destroy(&mCondv);
	}

	void Enqueue(T item)
	{
		pthread_mutex_lock(&mMutex);
		mQueue.push(item);
		pthread_cond_signal(&mCondv);
		pthread_mutex_unlock(&mMutex);
	}

	T Dequeue()
	{
		pthread_mutex_lock(&mMutex);
		while(mQueue.size() == 0)
			pthread_cond_wait(&mCondv, &mMutex);

		T item = mQueue.front();
		mQueue.pop();
		pthread_mutex_unlock(&mMutex);

		return item;
	}

private:

	std::queue<T>         mQueue;
    pthread_mutex_t  mMutex;
    pthread_cond_t   mCondv; 
};

#endif