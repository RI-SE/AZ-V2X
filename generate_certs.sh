#!/bin/bash
set -e

# Create directories for our certificates
mkdir -p ssl-certs
cd ssl-certs

# Generate CA private key and certificate
echo "Generating CA certificate..."
openssl genrsa -out ca.key 2048
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt -subj "/CN=Test CA/O=voiapp.io"
cat ca.crt ca.key > ca.pem

# Function to generate certificate for a component
generate_cert() {
    local NAME=$1
    echo "Generating certificate for $NAME..."
    
    # Generate private key
    openssl genrsa -out ${NAME}.key 2048
    
    # Generate certificate signing request (CSR)
    openssl req -new -key ${NAME}.key -out ${NAME}.csr \
        -subj "/CN=${NAME}.voiapp.io/O=voiapp.io"
    
    # Generate certificate signed by our CA
    openssl x509 -req -days 365 -in ${NAME}.csr \
        -CA ca.crt -CAkey ca.key -CAcreateserial \
        -out ${NAME}.crt
    
    # Create PEM file containing both certificate and private key
    cat ${NAME}.crt ${NAME}.key > ${NAME}.pem
    
    # Cleanup intermediate files
    rm ${NAME}.csr
}

# Generate certificates for server and client
generate_cert "server"
generate_cert "client"

# Set appropriate permissions
chmod 600 *.key *.pem
chmod 644 *.crt

echo "Certificate generation complete. Files created:"
ls -l

echo "
Generated files:
- ca.crt: CA certificate (public)
- ca.key: CA private key
- server.pem: Server certificate + private key
- client.pem: Client certificate + private key
- server.crt: Server certificate (public)
- client.crt: Client certificate (public)
- server.key: Server private key
- client.key: Client private key
"