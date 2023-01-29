/* semaphore descriptor type */
typedef struct semd_t {
	struct semd_t *s_next,	/* next element on the queue */
	*s_prev;	/* previous element on the queue */
	int *s_semAdd;	/* pointer to the semaphore proper */
	proc_link s_link;	/* pointer/index to the tail of the queue of processes blocked on this semaphore */
} semd _t;
