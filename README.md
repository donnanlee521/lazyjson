### lazyjson

This JSON parser fully utilizes C++20 STL libraries, postpones parsing value at actual using time of a value, reduces unnecessary parsing cost as possible as it is.

And this library exploits 'std::vector' container for both JSON array type and JSON dictionary type, and no memory allocation happens except for the container do,

consequently the parsed JSON memory layout has very compact scheme, which is highly cache friendly and memory effiecieny in real time environment.

##### basic usage
```cpp

/* test.json
{
  "status": "success",
  "meta": {
    "total_records": 2,
    "page": 1,
    "has_more": false
  },
  "data": [
    {
      "product_id": "prod_99ae3",
      "title": "Wireless Headphones",
      "price": 79.99,
      "in_stock": true,
      "dimensions": {
        "width": 15.2,
        "height": 30.523,
        "weight_oz": 3.14e+5
      }
  }],
  "etc": null
}
*/

using namespace std::string_view_literals;
...

lazy::json_container jc = lazy::json_container::from_file("test.json");

// declare json type as reference, or It will be copied.
lazy::json& jval = jc.get();

// for parsing string type using implicit cast
std::string_view status = jval["status"sv]; // == "success"

// for parsing int type
int64_t total_records = jval["meta"sv]["total_records"sv]; // == 2

// for parsing float type
double height = jval["data"sv][0]["dimensions"sv]["height"sv] // == 30.523

// for parsing null type..., json_null type is corresponding to c++ nullptr_t in this lib, and should explicitly cast except other json types(int, float, string, boolean...)
std::nullptr_t static_cast<std::nullptr_t>(jval["etc"sv]);

"
```
