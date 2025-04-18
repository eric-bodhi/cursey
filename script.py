with open("test.txt", "w") as file:
    for i in range(1, 100001):
        file.write("Line " + str(i) + "\n")
