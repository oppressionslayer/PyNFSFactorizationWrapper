""" To use this library, use:
    from sfactorint import p2ecm 

    To build ecm, from the primality test directory, simply do
    cd calculators
    make
    cd ..
    ipython3
    from sfactorint import p2ecm
"""

import math
import random
import re
from subprocess import Popen, PIPE

def p2ecm(N):
    try:
        process = Popen(["calculators/ecm", str(N)], stdout=PIPE)
    except:
        print("\n" * 2)
        print("calculators/ecm doesn't exist, the returned result will be incorrect")
        print("You can fix this by following the directions in the source or by:")
        print("cd calculators")
        print("make")
        print("\n")
        print("This should create ecm and you'll be able to use sfactorint.")
        print("Currently this works under linux, OSX, termux android app and")
        print("other distributions.")
        print("\n")
        print("The future version of this program will be moved to another repo:")
        print("sfactorint which will automatically compile Alpertons ecm so this")
        print("manual intervention is only temporary. I only included it because")
        print("i thought those that would take time to do the manual steps would")
        print("find it worth it to get access to Alpertons ECM factoring")
        return 0
    (output, err) = process.communicate()
    exit_code = process.wait()

    output_decoded = output.decode()
    factors = output_decoded.split('=')[1].split('<')[0].split('*')

    # Check if the number is a prime number
    prime_pattern = re.compile(r'prime', re.IGNORECASE)

    if prime_pattern.search(output_decoded):
        print(f"{N} is a prime number.")
        return [N]
    
    # Check if the number is a perfect square
    if math.isqrt(N)**2 == N:
        print(f"{N} is a perfect square.")
        base = math.isqrt(N)
        return [base, base]

    prevtemp = []
    for xx in factors:
        factorsstring = re.sub(r'\([^)]*\)', '', xx)
        multiple = factorsstring.find("^")
        if multiple > 0:
            answer = factorsstring[:multiple]
            repeat = factorsstring[multiple+1:]
            for x in range(int(repeat)):
                prevtemp.append(int(answer))
        else:
            prevtemp.append(int(factorsstring.replace(' ', '')))
    return prevtemp


