#!/usr/bin/env python3

import random
import sys

def generate_normal_numbers(n, output_file):
    with open(output_file, 'w') as file:
        for _ in range(n):
            number = random.randint(0, n)
            file.write(str(number) + '\n')

def generate_zipfian_numbers(n, alpha, output_file):
    with open(output_file, 'w') as file:
        population = [i for i in range(n)]
        weights = [1.0 / (i ** alpha) for i in range(1, n + 1)]
        total_weight = sum(weights)
        weights = [w / total_weight for w in weights]
        
        for _ in range(n):
            number = random.choices(population, weights=weights)[0] + 1
            file.write(str(number) + '\n')

if len(sys.argv) != 4:
    print("Usage: python random_generator.py <distribution_type> <n> <output_file>")
    print("Distribution types: normal, zipfian")
else:
    distribution_type = sys.argv[1]
    n = int(sys.argv[2])
    output_file = sys.argv[3]

    if distribution_type == 'normal':
        generate_normal_numbers(n, output_file)
        print("Normal distribution numbers generated and saved to", output_file)
    elif distribution_type == 'zipfian':
        alpha = 1.2  # Shape parameter for the Zipfian distribution
        generate_zipfian_numbers(n, alpha, output_file)
        print("Zipfian distribution numbers generated and saved to", output_file)
    else:
        print("Invalid distribution type. Available options: normal, zipfian")

