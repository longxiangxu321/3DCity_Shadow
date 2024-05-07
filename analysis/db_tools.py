import os
import sys
from sqlalchemy import create_engine, text

def get_db_parameters(file_name):
    location = os.path.dirname(os.path.abspath(__file__))
    parameters_file = os.path.join(location,file_name)

    with open (parameters_file, 'rt') as myfile:
        txt = myfile.read()
        params = txt.split()
    return params

def engineBuilder(file_name):
    user_db,password_db,host_db,port_db,database_db,schema_db = get_db_parameters(file_name)
    db_url = f'postgresql+psycopg2://{user_db}:{password_db}@{host_db}:{port_db}/{database_db}'
    print(db_url)
    # Create the sqlalchemy engine
    try:
        db_engine = create_engine(db_url, connect_args={'options': '-csearch_path={},public'.format(schema_db)}) # Searches left-to-right
        print(f'Connection to database {database_db} was successful')
    except:
        print(f'Connection to database {database_db} failed')
    return db_engine

def setup_connection(user,password,database,host,port):
    '''
    Set up connection to the given database
    Parameters:
    user --  username
    password -- password of user
    database -- database name
    host -- host address of database
    port -- port number of database
    '''
    try:
        print(f'\n>> Connecting to PostgreSQL database: {database}')
        return psycopg2.connect(database=database, user=user, password=password, host=host, port=port)

    except (Exception, psycopg2.Error) as error:
        print("Error while connecting to PostgreSQL;", error)
        sys.exit()

def close_connection(connection, cursor):
    """
    Close connection to the database and cursor used to perform queries.
    Parameters:
    connection -- database connection
    cursor -- cursor for database connection
    """

    if cursor:
        cursor.close()
        print("\n>> Cursor is closed")

    if connection:
        connection.close()
        print("\n>> PostgreSQL connection is closed")