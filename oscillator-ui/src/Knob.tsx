import React from 'react';

interface KnobProps {
  size?: number;
  min?: number;
  max?: number;
  step?: number;
  value: number;
  onChange: (value: number) => void;
  label?: string;
  displayValue?: boolean; // ✅ New prop
}

interface KnobState {
  dragging: boolean;
}

export default class Knob extends React.Component<KnobProps, KnobState> {
  static defaultProps = {
    size: 80,
    min: 0,
    max: 100,
    step: 1,
    label: '',
    displayValue: false, // ✅ Default to false
  };

  private svgRef = React.createRef<SVGSVGElement>();
  private centerX = 0;
  private centerY = 0;

  private static SWEEP = 352;
  private static START_RAW = 270;

  constructor(props: KnobProps) {
    super(props);
    this.state = { dragging: false };
  }

  componentDidMount() {
    const svg = this.svgRef.current!;
    const rect = svg.getBoundingClientRect();
    this.centerX = rect.left + rect.width / 2;
    this.centerY = rect.top + rect.height / 2;
    window.addEventListener('mousemove', this.onMouseMove);
    window.addEventListener('mouseup', this.onMouseUp);
  }

  componentWillUnmount() {
    window.removeEventListener('mousemove', this.onMouseMove);
    window.removeEventListener('mouseup', this.onMouseUp);
  }

  onMouseDown = (e: React.MouseEvent) => {
    e.preventDefault();
    this.setState({ dragging: true });
  };

  onMouseUp = () => {
    if (this.state.dragging) this.setState({ dragging: false });
  };

  onMouseMove = (e: MouseEvent) => {
    if (!this.state.dragging) return;
    const { min, max, step, onChange } = this.props;
    const dx = e.clientX - this.centerX;
    const dy = e.clientY - this.centerY;
    let raw = Math.atan2(dy, dx) * (180 / Math.PI);
    raw = raw < 0 ? raw + 360 : raw;
    let degFromUp = (raw - Knob.START_RAW + 360) % 360;
    const angle = Math.max(0, Math.min(Knob.SWEEP, degFromUp));
    const percent = angle / Knob.SWEEP;
    const rawValue = min! + percent * (max! - min!);
    const stepped = Math.round(rawValue / step!) * step!;
    onChange(stepped);
  };

  render() {
    const { size, min, max, value, label, displayValue } = this.props;

    const pct = (value - min!) / (max! - min!);
    const angle = pct * Knob.SWEEP;
    const drawRaw = (Knob.START_RAW + angle) % 360;

    const indicatorLength = (size! - 20) / 2;
    const theta = (drawRaw * Math.PI) / 180;
    const x2 = size! / 2 + Math.cos(theta) * indicatorLength;
    const y2 = size! / 2 + Math.sin(theta) * indicatorLength;

    return (
      <div className="flex flex-col items-center select-none">
        <svg
          ref={this.svgRef}
          width={size}
          height={size}
          onMouseDown={this.onMouseDown}
          className="cursor-pointer"
        >
          <circle
            cx={size! / 2}
            cy={size! / 2}
            r={(size! - 10) / 2}
            stroke="#444"
            strokeWidth="10"
            fill="none"
          />
          <line
            x1={size! / 2}
            y1={size! / 2}
            x2={x2}
            y2={y2}
            stroke="#222"
            strokeWidth="4"
            strokeLinecap="round"
          />
          <circle cx={x2} cy={y2} r={8} fill="#fff" />

          {displayValue && (
            <text
              x="50%"
              y="50%"
              dominantBaseline="middle"
              textAnchor="middle"
              fontSize={size! / 5}
              fill="#fff"
            >
              {value}
            </text>
          )}
        </svg>

        {label && <div className="mt-2 text-sm font-medium text-black">{label}</div>}
      </div>
    );
  }
}
