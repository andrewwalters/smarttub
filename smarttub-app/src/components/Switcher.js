import React, { useState } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import { FormControlLabel, Switch, makeStyles } from '@material-ui/core';
import { selectSetting } from '../redux/selectors';
import { setSetting } from '../redux/actions';

const useStyles = makeStyles({
    root: {
        display: "flex",
        justifyContent: "space-between",
        marginLeft: 10,
        marginRight: 10
    },
});

export function Switcher({ label, param }) {
    const classes = useStyles();
    const value = useSelector(selectSetting(param));
    const [checked, setChecked] = useState(Boolean(value));
    const dispatch = useDispatch();
    const onChange = event => {
        console.log(event.target.checked, event);
        setChecked(Boolean(event.target.checked));
        dispatch(setSetting(param, Number(event.target.checked)));
    };
    return (
        <FormControlLabel
            control={<Switch checked={checked} onChange={onChange} />}
            label={label}
            labelPlacement="start"
            classes={classes}
        />
    );
}
