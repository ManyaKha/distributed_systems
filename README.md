# Distributed systems project 2020

## Documentation
Our shared [drive](https://drive.google.com/drive/folders/1pJ7_yBAVj_Qdc6GxyntaDXKxpJl-cfUm?usp=sharing) for documentation

## Configuration
server port: 7777

## Data storage schema on the server
All the data is stored in a directory called **storage**. In the storage directory each user will have their own directory named with their username. In each user directory there will be files with named like the files which the user published and the content of these files will be their description.  
**Example:** 
 user xyz published a.txt and b.jpg files  
The two files will be found under these paths:  
**storage/xyz/a.txt**  
**storage/xyz/b.jpg** 
