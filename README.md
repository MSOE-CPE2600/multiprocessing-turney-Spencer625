# System Programming Lab 11 Multiprocessing
## Spencer Thacker <thackers@msoe.edu>

The goal of this assignment was to modify the Mandel program to generate a movie and divide the workload among child processes.


For my implementation, I decided to use semaphore to manage the child processes, as noted. The solution is simple and works quite well.


First, Semaphore limits the number of processes, and we then enter a for loop. In this loop, a fork statement is used to make a single child node, with the child node entering the if statement.


If you are the child node, you generate the image. The modified generation statement is dependent on the current frame that the statement had iterated into. Each parameter for the image is generated based on the given values. Once the image is made and released, sem_post is used to release the current semaphore and then exit the child process.


If you are the parent node, you iterate through the for loop once again making another child node. This repeats, increasing the number of child nodes until you are stopped by sem_wait, which means that the maximum number of child nodes is currently working. When sem_post is reached within a child process, the parent is allowed to make another child. The patient continues until all 0 - x numbers of frames are generated. At this point, the parent node waits for all child processes. Afterward, we close the Semaphore and unlink it.

## Runtime results
![Alt text](Runtime Vs Child Processes.png)


The figure above shows the Runtime over the number of Child processes for our default case, an "infinite" loop in Seahorse Valley. The trend line for the graph reveals that this is an Inverse Relationship between the Runtime number of Child processes. Our R^2 value tells us that the equation found in the figure is a good fit for predicting future values.

In conclusion, we were able to reduce runtime by distributing the task among child processes. The final movies can be found in “mandel.mpg”. It attempts to recreate an infinite loop in a location called “Sea Horse Valley”, but sadly I could not get it right.


