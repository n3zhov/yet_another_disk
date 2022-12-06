CREATE SCHEMA IF NOT EXISTS yet_another_disk;

CREATE  TABLE yet_another_disk.system_items (
                                                id                   uuid  NOT NULL  ,
                                                url                  char(255)    ,
                                                parent_id            uuid    ,
                                                item_type            char(255)    ,
                                                item_size            bigint    ,
                                                "date-time"          timestamptz    ,
                                                id_string            char(255)    ,
                                                parent_string        char(255)    ,
                                                CONSTRAINT pk_system_items PRIMARY KEY ( id )
);

CREATE  TABLE yet_another_disk.history (
                                           item_id              uuid    ,
                                           "date-time"          timestamptz    ,
                                           parent_id            uuid    ,
                                           item_size            bigint    ,
                                           url                  char(255)
);

ALTER TABLE yet_another_disk.history ADD CONSTRAINT fk_history_system_items FOREIGN KEY ( item_id ) REFERENCES yet_another_disk.system_items( id ) ON DELETE CASCADE;
