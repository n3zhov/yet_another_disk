import pytest
import json
import re
import subprocess
import sys
from testsuite.databases import pgsql


# Start the tests via `make test-debug` or `make test-release`

API_BASEURL = "http://localhost:8080"

ROOT_ID = "069cb8d7-bbdd-47d3-ad8f-82ef4c269df1"

IMPORT_BATCHES = [
    {
        "items": [
            {
                "type": "FOLDER",
                "id": "069cb8d7-bbdd-47d3-ad8f-82ef4c269df1",
                "parentId": None
            }
        ],
        "updateDate": "2022-02-01T12:00:00+0000"
    },
    {
        "items": [
            {
                "type": "FOLDER",
                "id": "d515e43f-f3f6-4471-bb77-6b455017a2d2",
                "parentId": "069cb8d7-bbdd-47d3-ad8f-82ef4c269df1"
            },
            {
                "type": "FILE",
                "url": "/file/url1",
                "id": "863e1a7a-1304-42ae-943b-179184c077e3",
                "parentId": "d515e43f-f3f6-4471-bb77-6b455017a2d2",
                "size": 128
            },
            {
                "type": "FILE",
                "url": "/file/url2",
                "id": "b1d8fd7d-2ae3-47d5-b2f9-0f094af800d4",
                "parentId": "d515e43f-f3f6-4471-bb77-6b455017a2d2",
                "size": 256
            }
        ],
        "updateDate": "2022-02-02T12:00:00+0000"
    },
    {
        "items": [
            {
                "type": "FOLDER",
                "id": "1cc0129a-2bfe-474c-9ee6-d435bf5fc8f2",
                "parentId": "069cb8d7-bbdd-47d3-ad8f-82ef4c269df1"
            },
            {
                "type": "FILE",
                "url": "/file/url3",
                "id": "98883e8f-0507-482f-bce2-2fb306cf6483",
                "parentId": "1cc0129a-2bfe-474c-9ee6-d435bf5fc8f2",
                "size": 512
            },
            {
                "type": "FILE",
                "url": "/file/url4",
                "id": "74b81fda-9cdc-4b63-8927-c978afed5cf4",
                "parentId": "1cc0129a-2bfe-474c-9ee6-d435bf5fc8f2",
                "size": 1024
            }
        ],
        "updateDate": "2022-02-03T12:00:00+0000"
    },
    {
        "items": [
            {
                "type": "FILE",
                "url": "/file/url5",
                "id": "73bc3b36-02d1-4245-ab35-3106c9ee1c65",
                "parentId": "1cc0129a-2bfe-474c-9ee6-d435bf5fc8f2",
                "size": 64
            }
        ],
        "updateDate": "2022-02-03T15:00:00+0000"
    }
]

EXPECTED_TREE = {
    "type": "FOLDER",
    "id": "069cb8d7-bbdd-47d3-ad8f-82ef4c269df1",
    "size": 1984,
    "url": None,
    "parentId": None,
    "date": "2022-02-03T15:00:00+0000",
    "children": [
        {
            "type": "FOLDER",
            "id": "1cc0129a-2bfe-474c-9ee6-d435bf5fc8f2",
            "parentId": "069cb8d7-bbdd-47d3-ad8f-82ef4c269df1",
            "size": 1600,
            "url": None,
            "date": "2022-02-03T15:00:00+0000",
            "children": [
                {
                    "type": "FILE",
                    "url": "/file/url3",
                    "id": "98883e8f-0507-482f-bce2-2fb306cf6483",
                    "parentId": "1cc0129a-2bfe-474c-9ee6-d435bf5fc8f2",
                    "size": 512,
                    "date": "2022-02-03T12:00:00+0000",
                    "children": None,
                },
                {
                    "type": "FILE",
                    "url": "/file/url4",
                    "id": "74b81fda-9cdc-4b63-8927-c978afed5cf4",
                    "parentId": "1cc0129a-2bfe-474c-9ee6-d435bf5fc8f2",
                    "size": 1024,
                    "date": "2022-02-03T12:00:00+0000",
                    "children": None
                },
                {
                    "type": "FILE",
                    "url": "/file/url5",
                    "id": "73bc3b36-02d1-4245-ab35-3106c9ee1c65",
                    "parentId": "1cc0129a-2bfe-474c-9ee6-d435bf5fc8f2",
                    "size": 64,
                    "date": "2022-02-03T15:00:00+0000",
                    "children": None
                }
            ]
        },
        {
            "type": "FOLDER",
            "id": "d515e43f-f3f6-4471-bb77-6b455017a2d2",
            "parentId": "069cb8d7-bbdd-47d3-ad8f-82ef4c269df1",
            "size": 384,
            "url": None,
            "date": "2022-02-02T12:00:00+0000",
            "children": [
                {
                    "type": "FILE",
                    "url": "/file/url1",
                    "id": "863e1a7a-1304-42ae-943b-179184c077e3",
                    "parentId": "d515e43f-f3f6-4471-bb77-6b455017a2d2",
                    "size": 128,
                    "date": "2022-02-02T12:00:00+0000",
                    "children": None
                },
                {
                    "type": "FILE",
                    "url": "/file/url2",
                    "id": "b1d8fd7d-2ae3-47d5-b2f9-0f094af800d4",
                    "parentId": "d515e43f-f3f6-4471-bb77-6b455017a2d2",
                    "size": 256,
                    "date": "2022-02-02T12:00:00+0000",
                    "children": None
                }
            ]
        },
    ]
}


def deep_sort_children(node):
    if node.get("children"):
        node["children"].sort(key=lambda x: x["id"])

        for child in node["children"]:
            deep_sort_children(child)


def print_diff(expected, response):
    with open("expected.json", "w") as f:
        json.dump(expected, f, indent=2, ensure_ascii=False, sort_keys=True)
        f.write("\n")

    with open("response.json", "w") as f:
        json.dump(response, f, indent=2, ensure_ascii=False, sort_keys=True)
        f.write("\n")

    subprocess.run(["git", "--no-pager", "diff", "--no-index",
                    "expected.json", "response.json"])


async def test_imports(service_client):
    for index, batch in enumerate(IMPORT_BATCHES):
        print(f"Importing batch {index}")
        response = await service_client.post("/imports", json=batch)

        assert response.status == 200, \
            f"Expected HTTP status code 200, got {response.status}"

    response = await service_client.get(f"/nodes/{ROOT_ID}")

    json_response = json.loads(response.text)
    deep_sort_children(json_response)
    deep_sort_children(EXPECTED_TREE)
    assert response.status == 200, \
        f"Expected HTTP status code 200, got {response.status}"
    assert json_response == EXPECTED_TREE, \
        "Expected tree doesn't match result of imports/nodes"


async def test_db_delete(service_client):
    for index, batch in enumerate(IMPORT_BATCHES):
        print(f"Importing batch {index}")
        response = await service_client.post("/imports", json=batch)

        assert response.status == 200, \
            f"Expected HTTP status code 200, got {response.status}"

    response = await service_client.delete(f'/delete/{ROOT_ID}',
                                           params={'date': '2022-06-26T21:12:01.000Z'})
    assert response.status == 200

    response = await service_client.get(f'/nodes/{ROOT_ID}')
    assert response.status == 404, \
        f"Expected HTTP status code 404, got {response.status}"


@pytest.mark.pgsql('db-1', files=['initial_data.sql'])
async def test_db_initial_data(service_client):
    response = await service_client.get(f"/nodes/{ROOT_ID}")
    json_response = json.loads(response.text)
    deep_sort_children(json_response)
    deep_sort_children(EXPECTED_TREE)
    assert response.status == 200, \
        f"Expected HTTP status code 200, got {response.status}"
    assert json_response == EXPECTED_TREE, \
        "Expected tree doesn't match result of imports/nodes"
