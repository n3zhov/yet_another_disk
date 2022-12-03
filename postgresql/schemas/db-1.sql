CREATE SCHEMA IF NOT EXISTS yet_another_disk;

CREATE TABLE IF NOT EXISTS yet_another_disk.system_items (
                                                             url                  varchar[]    ,
                                                             "date-time"          date    ,
                                                             parent_id            varchar[]    ,
                                                             item_type            varchar[]    ,
                                                             item_size            bigint    ,
                                                             children             text[]    ,
                                                             id                   varchar[]  NOT NULL  ,
                                                             CONSTRAINT pk_system_items PRIMARY KEY ( id )
);


CREATE TABLE IF NOT EXISTS yet_another_disk.history (
                                                        id                   numeric(20,0)  NOT NULL  ,
                                                        "date-time"          date    ,
                                                        parent_id            varchar[]    ,
                                                        item_type            varchar[]    ,
                                                        item_size            bigint    ,
                                                        children             text[]    ,
                                                        url                  varchar[]    ,
                                                        item_id              varchar[]    ,
                                                        CONSTRAINT pk_history PRIMARY KEY ( id )
);



