# Bakery-Concurrency-Simulation
This project simulates the operation of a bakery named Sweet Harmony, where customers wearing red and blue outfits must maintain a 1:1 ratio inside the bakery at all times. The bakery also has a limited number of tables.

Visuals: https://www.canva.com/design/DAGnjZTpAzg/y-DWVWGKGk79uVQq90hVzg/view?utm_content=DAGnjZTpAzg&utm_campaign=designshare&utm_medium=link&utm_source=viewer


###### Project Title: Sweet Harmony



###### Problem Statement

In the small, picturesque town of Pastelville, there is a lovely bakery called "Sweet Harmony."
This bakery is famous for its scrumptious pastries and charming atmosphere. The bakery has a
limited number of tables for customers to sit and enjoy their treats. To maintain the pleasant
ambiance, they have a unique rule: at any given time, there can only be an equal number of
customers wearing red and blue outfits inside the bakery.

**Sweet Harmony's staff must manage the following aspects:**

1. Allowing customers to enter the bakery while maintaining the equal number of red
and blue outfits rule.
2. Ensuring customers can find a table to sit at or wait for a table to become available.
3. Managing the queue of customers waiting outside the bakery when the equal outfit
rule cannot be maintained.
4. Handling customers leaving the bakery, freeing up tables, and allowing new
customers to enter.

**A C program can be developed using semaphores, processes, and thread mutex locks to
address the above challenges:**

1. Use Semaphores to manage the equal outfit rule, allowing only customers with
matching outfits to enter.
2. Use Processes to represent individual customers and their actions
3. Use Thread mutex locks used to synchronize the available tables, ensuring customers
can only sit at a free table.
4. Use semaphores to manage the waiting queue outside the bakery.

By utilizing semaphores, processes, and thread mutex locks, the C program can effectively
simulate the charming environment at Sweet Harmony bakery, while addressing the
concurrency issues and unique outfit rule that arise in this scenario.


###### Introduction

Using Threads, mutex locks and semaphore, I implemented a solution that synchronizes the
activities of customer in bakery. This solution ensures that customers are entered the bakery by

maintaining equal number of red and blue outfits rule. It also ensures that customers find a

table to sit and waiting outside the bakery when equal outfit rule cannot be maintained.

###### Project Description

**Overview**

This project simulates a **bakery environment** where two types of customers — **Red** and **Blue** —

arrive and attempt to dine at a limited number of tables. Using **POSIX threads** , **mutexes** , and

**semaphores** , the simulation enforces **mutual exclusion** , **fairness** , and **controlled resource**

**access** , representing real-world concurrency challenges in a shared resource environment.

**Purpose**

To demonstrate how multithreading and synchronization primitives can be used to:

1. Control access to limited resources (tables),
2. Avoid race conditions in a shared environment,
3. Maintain fairness between competing groups (Red and Blue customers),
4. Simulate real-world coordination using thread-safe logic.

**Key Features**

```
Threaded Customer Simulation: Each red/blue customer is a separate thread.
Entry Logic: A customer may only enter if their group has fewer people inside than the
other, or if the bakery is empty.
Resource Limiting: Semaphore (sem_t) ensures that only a limited number of customers
occupy tables simultaneously.
Fairness Enforcement: No group can dominate — controlled by red_inside and blue_inside
counters.
```

```
Mutex Protection: Shared data (like counters) is protected with a mutex to prevent race
conditions.
```
**User Inputs**

At the beginning of the program, the user specifies:

```
 Number of tables available in the bakery.
 Number of red and blue customers.
 Time each customer spends eating.
```
**Output**

During execution:

```
 Entry attempts and success are logged.
 Waiting for tables and eating actions are displayed.
 Exit logs show updates in customer counts.
```
After execution:

```
 A summary of all customers served is printed.
```
###### Methodology

**a) Inputs Collected:**

1. Number of tables
2. Number of Red customers
3. Number of Blue customers
4. Eating time (in seconds)

**b) Thread Creation:**

```
 Separate threads are created for each Red and Blue customer using pthread_create().
```
**c) Mutual Exclusion:**

```
 A mutex (pthread_mutex_t) protects shared counters like red_inside, blue_inside, etc.
```

**d) Semaphore:**

```
 A semaphore (sem_t table_sem) manages the number of available
tables.
 sem_wait() is used to wait for a free table.
 sem_post() is called when a customer finishes eating.
```
**e) Fairness Logic:**

```
 A Red customer may enter only if:
o red_inside < blue_inside
o OR the bakery is empty
 Similarly, Blue customers follow the same logic in reverse.
 This ensures turn-taking and avoids starvation of either group.
```
**f) Exit and Cleanup:**

```
 All threads are joined using pthread_join().
 Mutexes and semaphores are destroyed before the program ends.
```
##### Methods

```
 pthread_create() is used to spawn customer threads.
 pthread_join() ensures all threads complete before program termination.
 pthread_mutex_lock() ensures only one thread modifies shared state at a time.
 pthread_mutex_unlock() releases the lock after updates.
 sem_init() initializes the semaphore to the number of tables.
 sem_wait() blocks if no tables are available.
 sem_post() signals that a table has been freed.
```
**Thread Execution Flow**

1. **Entry Check:** Tries to enter based on fairness logic.
2. **Wait for Table:** Uses sem_wait() to acquire a table.
3. **Eat:** Sleeps for a user-defined time.
4. **Exit:** Updates counters and releases the table (sem_post()).


### Process Flowchart


### Source Code

1. #include <stdio.h>
2. #include <stdlib.h>
3. #include <pthread.h>
4. #include <semaphore.h>
5. #include <unistd.h>
6. #include <string.h>
7. int RED_COUNT = 3;
8. int BLUE_COUNT = 3;
9. int TABLES = 2;
10. int EATING_TIME = 1;
11. int red_inside = 0, blue_inside = 0;
12. int red_served = 0, blue_served = 0;
13. pthread_mutex_t mutex;
14. sem_t table_sem;
15. void* red_customer(void* arg)
16. {
17. int id = *(int*)arg;
18. free(arg);
19. while (1)
20. {
21. pthread_mutex_lock(&mutex);
22. if (red_inside < blue_inside)
23. {
24. red_inside++;
25. printf("🔴 Red %d entered (R=%d, B=%d)\n", id, red_inside, blue_inside);
26. pthread_mutex_unlock(&mutex);
27. break;
28. }
29. else if (red_inside == 0 && blue_inside == 0)
30. {
31. red_inside++;
32. printf("🔴 Red %d (first customer) entered (R=%d, B=%d)\n", id, red_inside, blue_inside);
33. pthread_mutex_unlock(&mutex);
34. break;
35. }
36. pthread_mutex_unlock(&mutex);
37. printf("🔴 Red %d waiting to enter...\n", id);


38. usleep(100000);
39. }
40. printf("🔴 Red %d waiting for a table...\n", id);
41. sem_wait(&table_sem);
42. printf("🔴 Red %d got a table\n", id);
43. sleep(EATING_TIME);
44. printf("🔴 Red %d leaving\n", id);
45. pthread_mutex_lock(&mutex);
46. red_inside--;
47. red_served++;
48. pthread_mutex_unlock(&mutex);
49. sem_post(&table_sem);
50. return NULL;
51. }
52. void* blue_customer(void* arg)
53. {
54. int id = *(int*)arg;
55. free(arg);
56. while (1)
57. {
58. pthread_mutex_lock(&mutex);
59. if (blue_inside < red_inside)
60. {
61. blue_inside++;
62. printf("🔵 Blue %d entered (R=%d, B=%d)\n", id, red_inside, blue_inside);
63. pthread_mutex_unlock(&mutex);
64. break;
65. }
66. else if (red_inside == 0 && blue_inside == 0)
67. {
68. blue_inside++;
69. printf("🔵 Blue %d (first customer) entered (R=%d, B=%d)\n", id, red_inside,
    blue_inside);
70. pthread_mutex_unlock(&mutex);
71. break;
72. }
73. pthread_mutex_unlock(&mutex);


74. printf("🔵 Blue %d waiting to enter...\n", id);
75. usleep(100000);
76. }
77. printf("🔵 Blue %d waiting for a table...\n", id);
78. sem_wait(&table_sem);
79. printf("🔵 Blue %d got a table\n", id);
80. sleep(EATING_TIME);
81. printf("🔵 Blue %d leaving\n", id);
82. pthread_mutex_lock(&mutex);
83. blue_inside--;
84. blue_served++;
85. pthread_mutex_unlock(&mutex);
86. sem_post(&table_sem);
87. return NULL;}
88. int get_positive_integer(const char* prompt)
89. {
90. int value = 0;
91. char input[100];
92. while (1)
93. {
94. printf("%s", prompt);
95. if (fgets(input, sizeof(input), stdin) == NULL)
96. {
97. printf("Error reading input. Using default value.\n");
98. return 1;
99. }
100. value = atoi(input);
101. if (value > 0) {
102. break;
103. } else {
104. printf("Please enter a positive integer.\n");}}
105. return value;
106. }
107. int main()
108. {
109. printf("🍰 Bakery Simulation Setup 🍰\n\n");
110. TABLES = get_positive_integer("Enter number of tables: ");
111. RED_COUNT = get_positive_integer("Enter number of red customers: ");
112. BLUE_COUNT = get_positive_integer("Enter number of blue customers: ");
113. EATING_TIME = get_positive_integer("Enter eating time (in seconds): ");
114. printf("\n🍰 Starting Bakery Simulation 🍰\n");
115. printf("Red customers: %d\n", RED_COUNT);


116. printf("Blue customers: %d\n", BLUE_COUNT);
117. printf("Available tables: %d\n", TABLES);
118. printf("Eating time: %d second(s)\n\n", EATING_TIME);
119. pthread_t red[RED_COUNT], blue[BLUE_COUNT];
120. pthread_mutex_init(&mutex, NULL);
121. sem_init(&table_sem, 0, TABLES);
122. for (int a = 0; a < RED_COUNT; a++)
123. {
124. int* id = malloc(sizeof(int));
125. if (id == NULL) {
126. perror("Error allocating memory");
127. return 1;
128. }
129. *id = a + 1;
130. if (pthread_create(&red[a], NULL, red_customer, id) != 0) {
131. perror("Error creating red customer thread");
132. free(id);
133. return 1;
134. }
135. usleep(100000);
136. }
137. for (int b = 0; b < BLUE_COUNT; b++)
138. {
139. int* id = malloc(sizeof(int));
140. if (id == NULL) {
141. perror("Error allocating memory");
142. return 1;
143. }
144. *id = b + 1;
145. if (pthread_create(&blue[b], NULL, blue_customer, id) != 0) {
146. perror("Error creating blue customer thread");
147. free(id);
148. return 1;
149. }
150. usleep(100000);
151. }
152. for (int a = 0; a < RED_COUNT; a++)
153. {
154. pthread_join(red[a], NULL);
155. }
156. for (int b = 0; b < BLUE_COUNT; b++)
157. {
158. pthread_join(blue[b], NULL);}


159. pthread_mutex_destroy(&mutex);
160. sem_destroy(&table_sem);
161. printf("\n🎉 All customers served. Bakery closed.\n");
162. printf("Summary:\n");
163. printf("- Red customers served: %d\n", red_served);
164. printf("- Blue customers served: %d\n", blue_served);
165. printf("- Total customers: %d\n", red_served + blue_served);
166. return 0;}

# Output

Let, the total number of customer is 5 and the total number of table is 3. The

output is given below: -




#### Conclusion

This project effectively demonstrates the use of **threads** , **mutexes** , and

**semaphores** in a realistic scenario. It teaches the importance of:

```
 Safe access to shared resources
 Proper thread synchronization
 Designing fair algorithms to avoid starvation
```
The Bakery Simulation is a practical and extensible model for understanding

concurrency control and fairness in resource-constrained systems.


