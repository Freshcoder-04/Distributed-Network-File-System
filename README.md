# Distributed-Network-File-System

## Introduction
The Distributed Network File System (DNFS) is a fully functional file system designed from scratch by our team. It comprises three major components:

- **Clients**: Systems or users interacting with the network file system, initiating various file-related operations such as reading, writing, deleting, and more.
- **Naming Server**: Acts as the central coordination point, managing the directory structure and facilitating communication between clients and storage servers. It provides clients with crucial information about file locations.
- **Storage Servers**: Responsible for storing files, managing data persistence and distribution, and interacting with both clients and the naming server.

## Assumptions
- Maximum 3 clients could connect to the naming server.
- Maximum 5 storage servers could connect to the naming server.
- Naming Server (NS) is initialized first, then at least 1 storage server, and only then will NS start accepting clients.
- Any storage server provides all the paths to be exposed as starting with `/Dir1/Dir2/File.txt`. This will further be interpreted by the NS as `~/Dir1/Dir2/File.txt` and will be stored in the cumulative directory structure accordingly.
- Clients have already considered the storage pattern of NS and then provide paths.
- Implementation of tries has been done character by character and also allows SS to include files with ANY kind of character present in the ASCII.
- Users as clients only enter the operation names in capitals only (e.g., READ, DELETE, CREATE, etc.).
- WRITE operation actually appends data into the selected file (does not overwrite).
- All paths do not have whitespace characters in them.

## Specifications

### 1. Naming and Storage Servers

#### 1.1 Initialization
- **Naming Server (NM) Initialization**: Initializes the NM to manage the directory structure and maintain essential file location information.
- **Storage Server (SS) Initialization**: Each SS sends vital details to the NM upon initialization, including IP address, ports for NM and client connection, and a list of accessible paths.

#### 1.2 On Storage Servers (SS)
- **Dynamic Addition of SS**: SS can dynamically add their entries to the NM during execution.
- **Commands Issued by NM**: NM can instruct SS to create/delete files or directories and copy data between SS.
- **Client Interactions**: SS facilitate client operations such as reading, writing, and obtaining file information.

#### 1.3 On Naming Server (NM)
- **Storing SS Data**: NM stores critical information provided by SS upon connection.
- **Client Task Feedback**: NM provides timely feedback to clients upon completion of tasks.

### 2. Clients
- **Directory Mounting**: Clients request file operations by providing the file's path. NM locates the correct SS, providing client access to the requested resource.
- **Functionalities**: Clients can read, write, delete files/folders, list directory contents, and obtain additional file information.

### 3. Other Features

#### 3.1 Multiple Clients
- **Concurrent Client Access**: NM handles multiple client requests simultaneously without blocking.
- **Concurrent File Reading**: Multiple clients can read the same file simultaneously.

#### 3.2 Error Codes
- **Error Handling**: Define descriptive error codes for various client request scenarios, enhancing communication between NFS and clients.

#### 3.3 Search in Naming Servers
- **Efficient Search**: Optimize NM's search process using efficient data structures like Tries and Hashmaps to swiftly identify the correct SS for a given request.

#### 3.4 Redundancy/Replication
- **Failure Detection**: NM detects SS failures promptly, ensuring the NFS responds effectively to disruptions.

#### 3.5 Bookkeeping
- **Logging and Message Display**: Implement logging mechanism to record requests and acknowledgments, aiding in debugging and system monitoring.
- **IP Address and Port Recording**: Log includes relevant communication information for traceability and issue diagnosis.

## Usage Directions
- To start the SS, run `./SS <IP>` from the respective folder, providing the IP address as a command-line argument.
- Clients interact with NM to access the NFS. Provide file paths for operations like reading, writing, deleting, etc.
- NM orchestrates communication between clients and SS, ensuring seamless file operations within the network file system.

---