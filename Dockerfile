# Use Python 3.10 slim image
FROM python:3.10-slim-bullseye

# Install system dependencies for building cubiomes
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    git \
    make \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy all files
COPY . .

# Run build script to compile cubiomes and verify_build
RUN bash build.sh

# Install Python dependencies
RUN pip install --no-cache-dir -r requirements.txt

# Expose port (Hugging Face default)
EXPOSE 7860

# Command to run the application
CMD ["python", "server.py"]
