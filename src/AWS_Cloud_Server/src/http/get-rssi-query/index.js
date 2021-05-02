const arc = require('@architect/functions');

// learn more about HTTP functions here: https://arc.codes/primitives/http
exports.handler = async function http (req) {
  let body = '';
  let status = 200;
  
  try{
    const key = req.query.source;
    if(!key) throw "missing key";
    let data = await arc.tables();
      
    var params = {
      IndexName: 'Index',
      KeyConditionExpression: 'HashKey = :hkey',
      ExpressionAttributeValues: {
        ':hkey': key
      }
    };

    const result = data.rssi.query(params);
    console.log(result);
    body = JSON.stringify(result.items);
  } catch(e){
    console.log(e);
    body = JSON.stringify({error: e});
    status = 400;
  }

  return {
    statusCode: status,
    headers: {
      'cache-control': 'no-cache, no-store, must-revalidate, max-age=0, s-maxage=0',
      'content-type': 'application/json; charset=utf8'
    },
    body
  }
}