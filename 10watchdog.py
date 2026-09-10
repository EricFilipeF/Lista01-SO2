import threading
import time
import random
from typing import Dict, List, Optional

class DeadlockWatchdog:
    def __init__(self, timeout: float = 5.0):
        self.timeout = timeout
        self.resources = {}
        self.threads = {}
        self.lock = threading.Lock()
        self.running = True
        self.watchdog_thread: Optional[threading.Thread] = None
        
    def register_resource(self, resource_id: str):       
        with self.lock:
            if resource_id not in self.resources:
                self.resources[resource_id] = {
                    'locked_by': None,
                    'lock': threading.Lock()
                }
    
    def register_thread(self, thread_id: str):
        with self.lock:
            if thread_id not in self.threads:
                self.threads[thread_id] = {
                    'last_progress': time.time(),
                    'held_resources': []
                }
    
    def acquire(self, thread_id: str, resource_id: str) -> bool:
        with self.lock:
            if thread_id not in self.threads:
                self.register_thread(thread_id)
            if resource_id not in self.resources:
                self.register_resource(resource_id)
        
        
        resource = self.resources[resource_id]
        
        
        if random.random() < 0.1:  
            time.sleep(0.1)       
        if resource['lock'].acquire(blocking=False):
            with self.lock:
                resource['locked_by'] = thread_id
                self.threads[thread_id]['held_resources'].append(resource_id)
                self.threads[thread_id]['last_progress'] = time.time()
            return True
        else:
            return False
    
    def release(self, thread_id: str, resource_id: str):
        
        with self.lock:
            resource = self.resources[resource_id]
            if resource['locked_by'] == thread_id:
                resource['locked_by'] = None
                resource['lock'].release()
                if resource_id in self.threads[thread_id]['held_resources']:
                    self.threads[thread_id]['held_resources'].remove(resource_id)
                self.threads[thread_id]['last_progress'] = time.time()
    
    def watchdog_loop(self):
        while self.running:
            time.sleep(self.timeout / 2)
            
            with self.lock:
                current_time = time.time()
                deadlocked_threads = []
                
                for thread_id, info in self.threads.items():
                    if current_time - info['last_progress'] > self.timeout:
                        deadlocked_threads.append(thread_id)
                
                if deadlocked_threads:
                    print("\nWatchdog: possivel deadlock detectado!")
                    print(f"  Threads suspeitas: {deadlocked_threads}")
                    for tid in deadlocked_threads:
                        resources = self.threads[tid]['held_resources']
                        print(f"  Thread {tid} mantém recursos: {resources}")
                    print("=" * 50)
    
    def start_watchdog(self):
        if self.watchdog_thread is None or not self.watchdog_thread.is_alive():
            self.running = True
            self.watchdog_thread = threading.Thread(target=self.watchdog_loop)
            self.watchdog_thread.daemon = True
            self.watchdog_thread.start()
            print("Watchdog iniciado")
    
    def stop_watchdog(self):
        self.running = False
        if self.watchdog_thread is not None and self.watchdog_thread.is_alive():
            self.watchdog_thread.join(timeout=2.0)
            print("Watchdog finalizado")

class DeadlockSimulator:
    def __init__(self, use_total_order: bool = False):
        self.use_total_order = use_total_order
        self.watchdog = DeadlockWatchdog(timeout=3.0)
        self.threads: List[threading.Thread] = []
        self.running = True
        
        
        self.resources = ['R1', 'R2', 'R3', 'R4', 'R5']
        for r in self.resources:
            self.watchdog.register_resource(r)
    
    def thread_work(self, thread_id: str, resource_order: List[str]):
        self.watchdog.register_thread(thread_id)
        
        while self.running:
            if self.use_total_order:
                resource_order = sorted(resource_order)
            
            acquired = []
            for resource in resource_order:
                if self.watchdog.acquire(thread_id, resource):
                    acquired.append(resource)
                    print(f"Thread {thread_id} adquiriu {resource}")
                    time.sleep(random.uniform(0.01, 0.03))
                else:
                    
                    for r in acquired:
                        self.watchdog.release(thread_id, r)
                    time.sleep(0.05)
                    break
            
            if len(acquired) == len(resource_order):
                print(f"Thread {thread_id} processando com recursos {acquired}")
                time.sleep(0.1)
                for resource in acquired:
                    self.watchdog.release(thread_id, resource)
                    print(f"Thread {thread_id} liberou {resource}")
            
            time.sleep(random.uniform(0.01, 0.1))
    
    def run_simulation(self, duration: float = 10):      
        print(f"\nSimulação de deadlock")
        print(f"  Ordem total: {self.use_total_order}")
        print("=" * 50)
        
        
        self.watchdog.start_watchdog()
        
        
        patterns = [
            ['R1', 'R2'],  # Thread 0
            ['R2', 'R3'],  # Thread 1
            ['R3', 'R4'],  # Thread 2
            ['R4', 'R5'],  # Thread 3
            ['R5', 'R1'],  # Thread 4 
        ]
        
        self.threads = []  
        for i, pattern in enumerate(patterns):
            t = threading.Thread(target=self.thread_work, 
                               args=(f"T{i}", pattern))
            t.daemon = True
            t.start()
            self.threads.append(t)
            
        print(f"Executando por {duration} segundos...")
        time.sleep(duration)
               
        self.running = False
        self.watchdog.stop_watchdog()
        
        for t in self.threads:
            if t is not None and t.is_alive():
                t.join(timeout=1.0)
        
        print("\nSimulação concluída")

def run_deadlock_experiment():
    print("Experimento de deadlock")
    print("=" * 60)
    
   
    print("\nSimulação sem ordem total (com deadlock)")
    sim = DeadlockSimulator(use_total_order=False)
    sim.run_simulation(duration=8)
    
    print("\nSimulação com ordem total (sem deadlock)")
    sim = DeadlockSimulator(use_total_order=True)
    sim.run_simulation(duration=8)

if __name__ == "__main__":
    run_deadlock_experiment()