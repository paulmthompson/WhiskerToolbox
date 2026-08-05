import os
import random

def generate_csv(filepath, num_frames=10, lines_per_frame=5, points_per_line=50):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    with open(filepath, 'w') as f:
        f.write('Frame,X,Y\n')
        for frame in range(num_frames):
            for _ in range(lines_per_frame):
                x_vals = [str(round(random.uniform(0, 100), 2)) for _ in range(points_per_line)]
                y_vals = [str(round(random.uniform(0, 100), 2)) for _ in range(points_per_line)]
                
                x_str = ','.join(x_vals)
                y_str = ','.join(y_vals)
                
                f.write(f'{frame},"{x_str}","{y_str}"\n')

if __name__ == "__main__":
    generate_csv('line_test_large.csv', num_frames=10, lines_per_frame=5, points_per_line=50)
    print("Generated line_test_large.csv")
