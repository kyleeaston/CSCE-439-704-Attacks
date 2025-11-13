import os 

input = "b64"
output = "padded-b64"

noise = bytearray([38, 101, 87, 43, 78, 89, 123, 51])


for name in os.listdir(input):
    in_path = os.path.join(input, name)
    out_path = os.path.join(output, f"{name}.bin")

    with open(in_path, "r") as f:
        b64text = f.read().strip()

    data = b64text.encode("ascii")  # convert string to bytes


    padded = bytearray()
    for i ,byte in enumerate(data):    
        padded.append(byte)
        padded.append(noise[i % len(noise)])

    with open(out_path, "wb") as f:
        f.write(padded)