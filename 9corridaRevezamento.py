import threading
import time
from typing import List

class RelayRace:
    def __init__(self, num_equipes: int, tam_equipe: int, num_pernas: int):
        self.num_equipes = num_equipes
        self.tam_equipe = tam_equipe
        self.num_pernas = num_pernas            
        self.barriers = []
        for _ in range(num_pernas):
            
            self.barriers.append({
                'count': 0,
                'mutex': threading.Lock(),
                'cond': threading.Condition(threading.Lock())
            })
        
        self.completed_rounds = 0
        self.completed_lock = threading.Lock()
        self.running = True
    
    def barrier_wait(self, barrier_id: int, team_id: int, member_id: int):        
        barrier = self.barriers[barrier_id]
        
        with barrier['mutex']:
            barrier['count'] += 1            
            if barrier['count'] == self.num_equipes:
                barrier['count'] = 0
                with barrier['cond']:
                    barrier['cond'].notify_all()
                return True  
        
        
        with barrier['cond']:
            while barrier['count'] > 0:
                barrier['cond'].wait()
        
        return False
    
    def team_thread(self, team_id: int, member_id: int):
        
        while self.running:            
            for leg in range(self.num_pernas):
                
                run_time = 0.1 + (team_id * 0.01)  
                time.sleep(run_time)
                
                
                print(f"Equipe {team_id}, Membro {member_id} "
                      f"completou perna {leg}")
                
                
                is_last = self.barrier_wait(leg, team_id, member_id)
                
                if is_last:
                    
                    if leg == self.num_pernas - 1:
                        with self.completed_lock:
                            self.completed_rounds += 1
                        print(f"Equipe {team_id} completou uma rodada!")
    
    def run_race(self, duration: float = 10):        
        print(f"Corrida de Revezamento")
        print(f"  {self.num_equipes} equipes, "
              f"{self.tam_equipe} membros/equipe, "
              f"{self.num_pernas} pernas")
        print("=" * 50)
                
        threads = []
        for team in range(self.num_equipes):
            for member in range(self.tam_equipe):
                t = threading.Thread(target=self.team_thread, 
                                   args=(team, member))
                t.start()
                threads.append(t)
               
        print(f"Correndo por {duration} segundos...")
        time.sleep(duration)
        
        self.running = False
        for t in threads:
            t.join(timeout=1)
        
        print("\n----ESTATÍSTICAS:")
        print(f"  Rodadas completadas: {self.completed_rounds}")
        print(f"  Taxa: {self.completed_rounds / (duration/60):.1f} rodadas/minuto")

def run_relay_experiment():
    configs = [
        (3, 2, 3),  
        (4, 2, 3),  
        (3, 3, 2),  
    ]
    
    for num_equipes, tam_equipe, num_pernas in configs:
        race = RelayRace(num_equipes, tam_equipe, num_pernas)
        print(f"\n{'='*60}")
        print(f"CONFIG: {num_equipes} equipes, {tam_equipe} membros, {num_pernas} pernas")
        race.run_race(duration=8)

if __name__ == "__main__":
    run_relay_experiment()