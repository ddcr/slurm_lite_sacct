import datetime
import calendar


def get_month_day_range(date_w_time):
    """
    For a datetime 'date_w_time' returns the start and end datetime
    for the month of 'date_w_time'.

    Month with 31 days:
    >>> date = datetime.date(2011, 7, 27)
    >>> get_month_day_range(date)
    (datetime.date(2011, 7, 1), datetime.date(2011, 7, 31))

    Month with 28 days:
    >>> date = datetime.date(2011, 2, 15)
    >>> get_month_day_range(date)
    (datetime.date(2011, 2, 1), datetime.date(2011, 2, 28))
    """
    first_day = date_w_time.replace(day=1,
                                    hour=0,
                                    minute=0,
                                    second=0)
    last_day = date_w_time.replace(
        day=calendar.monthrange(date_w_time.year, date_w_time.month)[1],
        hour=23,
        minute=59,
        second=59
    )
    return first_day.isoformat(sep='T'), last_day.isoformat(sep='T')


if __name__ == "__main__":
    # import doctest
    # doctest.testmod()
    # mintime=datetime.datetime.fromtimestamp(float(1306881101))
    # maxtime=datetime.datetime.fromtimestamp(float(1472488738))
    # '2011-05-31T19:31:41'
    # '2016-08-29T13:38:58'

    for y in [2011, 2012, 2013, 2014, 2015, 2016]:
        for m in range(1, 13):
            x = get_month_day_range(datetime.datetime(year=y, month=m, day=1))
            print(x)
        print('-'*80)
