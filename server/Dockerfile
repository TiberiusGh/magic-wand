# Use a lightweight Python image
FROM python:3.9-slim

# Set working directory inside the container
WORKDIR /app

# Install dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy the script
COPY magic_server.py .

# Open the UDP port (4210) so the Gateway can talk to us
EXPOSE 4210/udp

# Run the server with unbuffered output (-u) so logs show up instantly
CMD ["python", "-u", "magic_server.py"]

