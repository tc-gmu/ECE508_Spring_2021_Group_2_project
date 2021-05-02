# RSSI Server

This is a serverless project hosting the RSSI server. It is based on the [Architect](https://arc.codes/) framework. 

For testing, there are [Postman](https://www.postman.com/) collections in `postman`. 

## API

### Post data

`post /rssi/data` - send RSSI JSON data

required fields: 

``` json
{
    "source": "some_unique_identifier"
}
```

Anything else is optional. 

### Query data

`get /rssi/query/{{source_id}}` - find RSSI JSON data

Returns a JSON list of RSSI data from the specified source


## Deploying

To run locally, use 


``` sh
arc sandbox
```


To deploy, use 

``` sh
arc deploy [production]
```

and then get the URL that it returns from console. 

You will need a valid AWS credential profile, see the arc docs for more info. 