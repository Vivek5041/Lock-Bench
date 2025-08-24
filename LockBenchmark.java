import java.util.concurrent.locks.ReentrantLock;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.CyclicBarrier;
import java.util.concurrent.ThreadLocalRandom;

public class LockBenchmark {
    private static final int NUM_ITERATIONS = 10000000;
    private static final int NUM_WARMUP_ITERATIONS = 1000;
    private static final int NUM_RUNS = 5;
    
    // Add some work to do in critical section
    private static long sharedCounter = 0;
    private static ReentrantLock reentrantLock;
    private static final Object synchronizedLock = new Object();
    private static AtomicLong elapsed = new AtomicLong(0);
    
    // Add sleep times to simulate real-world scenarios
    private static final boolean SIMULATE_WORK = true;
    private static final int MAX_WORK_TIME_NANOS = 100; // 100ns max work time

    public static void main(String[] args) {
        if (args.length != 2) {
            System.out.println("usage: java LockBenchmark <num_threads> <lock_type>");
            System.out.println("lock_type: 1 for ReentrantLock, 2 for synchronized");
            System.exit(1);
        }

        int numWorkers = Integer.parseInt(args[0]);
        boolean useReentrantLock = args[1].equals("1");
        
        System.out.println("run,threads,lock_type,elapsed_seconds,ops_per_second,final_counter");
        
        for (int runCount = 0; runCount < NUM_RUNS; runCount++) {
            final int currentRun = runCount;
            elapsed.set(0);
            sharedCounter = 0;
            
            if (useReentrantLock) {
                reentrantLock = new ReentrantLock();
            }

            Thread[] threads = new Thread[numWorkers];
            final CyclicBarrier barrier = new CyclicBarrier(numWorkers);

            for (int i = 0; i < numWorkers; i++) {
                final int threadId = i;
                threads[i] = new Thread(() -> {
                    try {
                        runPhase(useReentrantLock, NUM_WARMUP_ITERATIONS);
                        barrier.await();

                        if (threadId == 0) {
                            elapsed.set(System.nanoTime());
                        }

                        runPhase(useReentrantLock, NUM_ITERATIONS);
                        barrier.await();

                        if (threadId == 0) {
                            long endTime = System.nanoTime();
                            elapsed.set(endTime - elapsed.get());
                        }

                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                });

                threads[i].start();
            }

            for (Thread thread : threads) {
                try {
                    thread.join();
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }

            double elapsedSeconds = elapsed.get() / 1_000_000_000.0;
            double operationsPerSecond = (NUM_ITERATIONS * numWorkers) / elapsedSeconds;
            
            System.out.printf("%d,%d,%s,%f,%f,%d%n",
                currentRun + 1,
                numWorkers,
                useReentrantLock ? "ReentrantLock" : "synchronized",
                elapsedSeconds,
                operationsPerSecond,
                sharedCounter);
        }
    }

    private static void runPhase(boolean useReentrantLock, int iterations) {
        ThreadLocalRandom random = ThreadLocalRandom.current();
        
        for (int j = 0; j < iterations; j++) {
            if (useReentrantLock) {
                reentrantLock.lock();
                try {
                    // Do some work in critical section
                    if (SIMULATE_WORK) {
                        simulateWork(random);
                    }
                    sharedCounter++;
                } finally {
                    reentrantLock.unlock();
                }
            } else {
                synchronized (synchronizedLock) {
                    // Do some work in critical section
                    if (SIMULATE_WORK) {
                        simulateWork(random);
                    }
                    sharedCounter++;
                }
            }
        }
    }
    
    private static void simulateWork(ThreadLocalRandom random) {
        // Simulate some work by spinning
        long workTime = random.nextLong(MAX_WORK_TIME_NANOS);
        long start = System.nanoTime();
        while (System.nanoTime() - start < workTime) {
            // Spin
        }
    }
}
