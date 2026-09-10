#!/usr/bin/env python3
"""按 bag 记录时间跳过开头并可选限制保留时长，保留消息内容及原始时间戳。"""

import argparse
from decimal import Decimal, InvalidOperation
from pathlib import Path

import genpy
import rosbag


def parse_seconds(parser, value, option, allow_zero=False):
    try:
        seconds = Decimal(value)
        if not seconds.is_finite() or seconds < 0 or (seconds == 0 and not allow_zero):
            raise ValueError()
        nanos = seconds * 1_000_000_000
        if nanos != nanos.to_integral_value():
            raise ValueError()
        return int(nanos)
    except (InvalidOperation, ValueError):
        constraint = '非负数' if allow_zero else '正数'
        parser.error(f'{option} 必须为{constraint}，精度不能超过纳秒')


def main():
    root = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path,
                        default=root / 'data/x_trimmed_1s.bag')
    parser.add_argument('--output', type=Path,
                        default=root / 'data/x_trimmed_1_05s.bag')
    parser.add_argument('--seconds', default='0.05',
                        help='从输入 bag 开头移除的秒数，0 表示不跳过（默认 0.05）')
    parser.add_argument('--duration',
                        help='从裁剪起点起保留的秒数（默认保留到文件末尾）')
    args = parser.parse_args()
    nanos = parse_seconds(parser, args.seconds, '--seconds', allow_zero=True)
    duration_ns = (parse_seconds(parser, args.duration, '--duration')
                   if args.duration is not None else None)

    if not args.input.is_file():
        parser.error(f'输入文件不存在：{args.input}')
    if args.input.resolve() == args.output.resolve():
        parser.error('输入和输出不能是同一个文件')

    with rosbag.Bag(str(args.input), 'r') as source:
        first = next(source.read_messages(raw=True), None)
        if first is None:
            parser.error('输入 bag 没有消息')
        cutoff = first.timestamp + genpy.Duration(
            nanos // 1_000_000_000, nanos % 1_000_000_000)
        end = (cutoff + genpy.Duration(
            duration_ns // 1_000_000_000, duration_ns % 1_000_000_000)
            if duration_ns is not None else None)
        kept = removed = 0
        # 独占创建输出文件，避免覆盖已有文件。
        try:
            output_file = args.output.open('xb')
        except FileExistsError:
            parser.error(f'输出文件已存在，拒绝覆盖：{args.output}')
        with output_file:
            with rosbag.Bag(output_file, 'w') as output:
                for topic, msg, timestamp, header in source.read_messages(
                        raw=True, return_connection_header=True):
                    if timestamp < cutoff:
                        removed += 1
                        continue
                    # 保留区间为 [cutoff, end)，结束时刻的消息不包含在内。
                    if end is not None and timestamp >= end:
                        break
                    output.write(topic, msg, timestamp, raw=True,
                                 connection_header=header)
                    kept += 1

    print(f'输出：{args.output}')
    print(f'跳过开头 {args.seconds} 秒（{removed} 条），保留 {kept} 条')
    if args.duration is not None:
        print(f'保留时长上限：{args.duration} 秒（不包含结束时刻）')
    print('原文件未修改，保留消息的时间戳未修改。')


if __name__ == '__main__':
    main()
