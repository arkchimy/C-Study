CREATE TABLE IF NOT EXISTS `monitorlog_202602`
(
    `no`        BIGINT NOT NULL AUTO_INCREMENT,
    `logtime`   DATETIME NOT NULL,
    `serverno`  INT NOT NULL,     -- 서버 고유 번호
    `type`      INT NOT NULL,     -- 모니터링 타입 (CPU, MEM, TPS 등)

    `avg`       DOUBLE NOT NULL,  -- 5분 평균값
    `min`       DOUBLE NOT NULL,  -- 5분 최소값
    `max`       DOUBLE NOT NULL,  -- 5분 최대값

    PRIMARY KEY (`no`),

    -- 조회용 인덱스 (모니터링에서 매우 중요)
    INDEX `idx_time` (`logtime`),
    INDEX `idx_server_time` (`serverno`, `logtime`),
    INDEX `idx_type_time` (`type`, `logtime`)
)
ENGINE=InnoDB;