import math
import operator

obstacles = [
					[ 2,2, 20 ],
					[ 3,2, 10 ],
					[ 0.5,0.5, 20 ],
					[ 1.25,4, 10 ],
					[ 3.5,3.5, 15 ],
					[ 4.5,1.25, 10 ],
					[ 2.75,0.5, 10 ],
					[ 0.5,3, 10 ],
					[ 2.5,4.75, 10 ]
				  ]

smallestX = -1
smallestY = -1
smallest = 1000

for x in range(0, len(obstacles)):
	for y in range(x + 1, len(obstacles)):
		xA,yA,rA = tuple(map(operator.mul, obstacles[x], (200,200,1)))
		xB,yB,rB = tuple(map(operator.mul, obstacles[y], (200,200,1)))
					
		dist = math.sqrt(math.pow(xB - xA, 2) + math.pow(yB - yA, 2)) - rA - rB
		
		print "APART:",x,y,dist
		
		if dist < smallest:
			smallestX = x
			smallestY = y
			smallest = dist

print "SMALLEST APART:", smallestX, smallestY,smallest

smallestF = -1
smallestFdist = 1000
smallestB = -1
smallestBdist = 1000
smallestL = -1
smallestLdist = 1000
smallestR = -1
smallestRdist = 1000

for x in range(0, len(obstacles)):
	xA,yA,rA = tuple(map(operator.mul, obstacles[x], (200,200,1)))
	
	distF = yA - rA
	distB = 1000 - yA - rA
	distL = xA - rA
	distR = 1000 - xA - rA
	
	print "CLOSE:",x,distF,distB,distL,distR
	
	if distF < smallestFdist:
		smallestF = x
		smallestFdist = distF

	if distB < smallestBdist:
		smallestB = x
		smallestBdist = distB

	if distL < smallestLdist:
		smallestL = x
		smallestLdist = distL

	if distR < smallestRdist:
		smallestR = x
		smallestRdist = distR

print "CLOSEST:",smallestF,smallestFdist,smallestB,smallestBdist,smallestL,smallestLdist,smallestR,smallestRdist
	
